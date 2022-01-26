#include "router.h"

#include <algorithm>
#include <cassert>
#include <queue>

#define FORMAT_HTTP_METHOD

#include "format.h"
#include "logger.h"
#include "utils.h"

using namespace cppmhd;

using cppmhd::global::empty;
using std::get;
using std::map;
using std::move;
using std::string;
using std::swap;
using std::vector;

namespace
{
class SimpleControllerWrapper : public HttpController
{
    SimpleControllerFunction fn_;

  public:
    SimpleControllerWrapper(SimpleControllerFunction &&fn) : fn_(fn) {}

    virtual void onRequest(HttpRequestPtr req, HttpResponsePtr &resp) override
    {
        resp = fn_(req);
    }

    virtual ~SimpleControllerWrapper() {}
};

struct ParserRegex {
    Regex normalStatic;
    Regex catchAll;
    Regex param;
    Regex route;
    Regex subRoute;

    static ParserRegex *regexes;

    ParserRegex()
    {
        MAYBE_UNUSED auto res =
            Regex::compileRegex(normalStatic, URL_STATIC_PART_PATTERN) && Regex::compileRegex(param, URL_REGEX_PATTERN)
            && Regex::compileRegex(subRoute, URL_SUBROUTE_PREFIX_PATTERN)
            && Regex::compileRegex(catchAll, URL_MATCHALL_PATTERN) && Regex::compileRegex(route, URL_ROUTE_PATTERN);
        assert(res);
    }
};

ParserRegex *ParserRegex::regexes = nullptr;

void destroyParserRegexes()
{
    if (ParserRegex::regexes != nullptr) {
        delete ParserRegex::regexes;

        ParserRegex::regexes = nullptr;
    }
}

ParserRegex &getRegex()
{
    if (ParserRegex::regexes == nullptr) {
        ParserRegex::regexes = new ParserRegex;
        std::atexit(destroyParserRegexes);
    }
    return *ParserRegex::regexes;
}

constexpr char PARAM_CHAR = ':';
constexpr char LEFT = '{';
constexpr char RIGHT = '}';


struct Param {
    RouterNodeType type;
    string prefix;

    // only for param node only
    string name;
    string pattern;
    Regex re;
};

struct PathParser {
    std::queue<Param> params;

    string calced;

    string actualFullPath;

  public:
    PathParser() = default;

    static bool parsePathSegment(PathParser &parser, const string &path);
};

string &operator/=(string &first, const std::string &second)
{
    return first.append("/").append(second);
}

string &operator/=(string &first, const Param &p)
{
    return first /= FORMAT("{}", p);
}


inline bool stringNCmp(const string &first, const string &second, size_t pos)
{
    return std::char_traits<char>::compare(first.c_str(), second.c_str(), pos) == 0;
}

inline int isPrefix(const string &whole, const string &prefix)
{
    return stringNCmp(whole, prefix, prefix.length());
}

inline size_t findUntilSlash(const string &in)
{
    auto end = 0u;
    for (; end < in.length() && in[end] != '/'; end++) {
    }
    return end;
}
inline size_t findLongestCommonPrefix(const string &first, const string &second)
{
    auto i = 0u;
    auto max = std::min(first.length(), second.length());
    for (; i < max && first[i] == second[i]; i++)
        ;
    return i;
}

inline int findWildcard(const string &path)
{
    auto i = 0u;
    for (; i < path.length(); i++) {
        if (path[i] == PARAM_CHAR) {
            return i;
        }
    }
    return -1;
}


struct RawNode {
    string path;
    string indices;
    vector<RawNode *> children;

    Handler handle;
    RouterNodeType type;

    Regex re;
    string name;

    bool wildchild{false};

    RawNode *indice(char c)
    {
        auto cstr = indices.c_str();
        for (auto i = 0u; i < indices.length(); i++) {
            if (cstr[i] == c) {
                assert(children.size() > i);
                return children[i];
            }
        }
        return nullptr;
    }

    RawNode &operator=(RawNode &&other)
    {
        swap(move(other));
        return *this;
    }
    RawNode() : type(RouterNodeType::ROOT) {}

    ~RawNode()
    {
        for (auto p : children) {
            delete p;
        }
    }

    void swap(RawNode &&other)
    {
        path.swap(other.path);
        indices.swap(other.indices);
        children.swap(other.children);
        handle.swap(other.handle);
        std::swap(type, other.type);
        re.swap(other.re);
        name.swap(other.name);
        std::swap(wildchild, other.wildchild);
    }

    RawNode(RawNode &&other)
    {
        swap(move(other));
    }

    RouterNode *buildChild()
    {
        RouterNode **node = nullptr;

        if (likely(children.size() > 0)) {
            auto c = new RouterNode *[children.size() + 1];
            for (auto i = 0u; i < children.size(); i++) {
                c[i] = children[i]->buildChild();
            }
            c[children.size()] = nullptr;
            node = c;
        }

        return new RouterNode(path, indices, node, move(handle), type, move(re), name, wildchild);
    }

    RouterNode *build()
    {
        return buildChild();
    }
};

struct RawTree {
    RawNode root;

    bool insertChild(RawNode *, string &, const string &, PathParser &, HttpControllerPtr);

    bool insert(RawNode *, string &, PathParser &, HttpControllerPtr);

    bool addRoute(const string &in, HttpControllerPtr ctrl);
};


struct RawBuilder {
    using MethodTree = std::tuple<HttpMethod, RawTree>;

    vector<MethodTree> trees_;

    RawBuilder() {}

    RawTree &getMethodTree(HttpMethod mth)
    {
        auto end = trees_.end();
        auto fn = [mth](const MethodTree &t) { return get<0>(t) == mth; };
        auto find = std::find_if(trees_.begin(), end, fn);

        if (unlikely(find == end)) {
            trees_.emplace_back(mth, RawTree());
            return getMethodTree(mth);
        } else {
            return get<1>(*find);
        }
    }

    bool insert(const string &path, HttpMethod mth, HttpControllerPtr ctrl)
    {
        LOG_TRACE("Add Route '{}' to '{}'", mth, path);
        return getMethodTree(mth).addRoute(path, ctrl);
    }

    void sort();
};

}  // namespace

namespace fmt
{
template <>
struct formatter<Param> {
    FMT_CONSTEXPR auto parse(format_parse_context &ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin();
        return it;
    }

    template <typename FormatContext>
    auto format(const Param &p, FormatContext &ctx) -> decltype(ctx.out())
    {
        if (p.type == RouterNodeType::CATCH_ALL) {
            return format_to(ctx.out(), "{}{}**:{}{}", p.prefix, LEFT, p.name, RIGHT);
        } else if (p.type == RouterNodeType::PARAM) {
            const char *fmt = "{}{}{}:{}{}";
            return format_to(ctx.out(), fmt, p.prefix, LEFT, p.pattern, p.name, RIGHT);
        } else {
            return format_to(ctx.out(), "error Param type");
        }
    }
};

template <>
struct formatter<RouterNodeType> {
    FMT_CONSTEXPR auto parse(format_parse_context &ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin();
        return it;
    }

    template <typename FormatContext>
    auto format(const RouterNodeType &p, FormatContext &ctx) -> decltype(ctx.out())
    {
        auto ptr = "unknown";

        switch (p) {
            case RouterNodeType::STATIC:
                ptr = "STATIC";
                break;
            case RouterNodeType::CATCH_ALL:
                ptr = "CATCH_ALL";
                break;
            case RouterNodeType::PARAM:
                ptr = "PARAM";
                break;
            case RouterNodeType::ROOT:
                ptr = "ROOT";
                break;
            default:
                break;
        }

        return format_to(ctx.out(), "{}", ptr);
    }
};

}  // namespace fmt


bool PathParser::parsePathSegment(PathParser &parser, const string &path)
{
    auto &rs = getRegex();
    if (unlikely(!rs.route.match(path))) {
        LOG_ERROR("Bad Route Pattern {}, skip", path);
        return false;
    }

    vector<string> parts;
    int i = 1;
    split(path, parts, "/");
    bool failed = true;

    string full;
    full.reserve(path.size() << 1);

    string calc;
    calc.reserve(path.length() << 1);

    bool warn = false;
    Regex::Group group;

    for (auto &part : parts) {
        group.clear();

        auto &p = parser.params;

        if (likely(rs.normalStatic.match(part))) {
            calc /= part;
            full /= part;
        } else if (unlikely(rs.catchAll.match(part, group))) {
            Param m;
            const auto &prefix = group["prefix"];

            calc = FORMAT("{}/{}{}", calc, prefix, PARAM_CHAR);

            m.prefix = move(prefix);
            m.name = move(group["name"]);
            m.type = RouterNodeType::CATCH_ALL;

            if (m.name.length() == 0) {
                m.name = FORMAT("{}", i);
                i++;
                warn = true;
            }

            full /= m;

            p.emplace(move(m));
        } else if (rs.param.match(part, group)) {
            Param m;
            m.type = RouterNodeType::PARAM;
            m.name = move(group["name"]);
            m.prefix = move(group["prefix"]);
            m.pattern = move(group["pattern"]);

            if (unlikely(m.name.length() == 0)) {
                m.name = FORMAT("{}", i);
                i++;
                warn = true;
            }

            if (unlikely(m.pattern.length() == 0)) {
                warn = true;
                m.pattern = URL_REGEX_DEFAULT_PATTERN;
            }

            auto c = Regex::compileRegex(m.re, m.pattern);

            if (likely(c)) {
                calc = FORMAT("{}/{}{}", calc, m.prefix, PARAM_CHAR);
                full /= m;

                p.emplace(move(m));
            } else {
                LOG_ERROR("Compile regex pattern '{}' in path '{}' failed. skip...", m.pattern, path);
                failed = false;
            }
        } else {
            LOG_ERROR("Unknown URI segment '{} of path '{}', skip...", part, path);
            failed = false;
        }
    }

    if (unlikely(!failed)) {
        return false;
    }

    if (path[path.length() - 1] == '/') {
        calc.append("/");
    }

    if (unlikely(warn)) {
        LOG_INFO("None-Standard Router Pattern '{}'. Change to '{}'", path, full);
    }

#ifndef NDEBUG
    {
        // count of -1 in calc must be equal with `parser.params.size()';
        size_t count = 0;
        auto ptr = calc.c_str();
        for (auto j = 0u; j < calc.length(); j++) {
            if (ptr[j] == PARAM_CHAR) {
                count++;
            }
        }
        assert(count == parser.params.size());
    }
#endif

    parser.calced.swap(calc);
    parser.actualFullPath.swap(full);

    return failed;
}

bool RawTree::addRoute(const string &in, HttpControllerPtr ctrl)
{
    PathParser p;

    if (unlikely(!PathParser::parsePathSegment(p, in)))
        return false;

    string save(p.calced);

    auto ret = insert(&root, save, p, ctrl);

    assert(!ret || p.params.size() == 0);

    return ret;
}

bool RawTree::insert(RawNode *node, string &path, PathParser &parser, HttpControllerPtr ctrl)
{
    const auto &fullPath = parser.actualFullPath;

    if (unlikely(node->path == "" && node->indices == "")) {
        insertChild(node, path, fullPath, parser, ctrl);
        return true;
    }

    while (true) {
        auto i = findLongestCommonPrefix(path, node->path);

        if (i < node->path.length()) {
            auto child = new RawNode;
            child->path = node->path.substr(i);
            child->type = node->type;
            child->re = move(node->re);
            child->indices = node->indices;
            child->children.swap(node->children);
            child->name = node->name;
            child->handle = move(node->handle);
            child->wildchild = node->wildchild;

            node->children.emplace_back(child);
            node->indices = string(node->path.c_str() + i, 1);
            node->path = string(path.c_str(), i);
            node->type = RouterNodeType::STATIC;
            node->wildchild = false;
        }

        if (i < path.length()) {
            path = path.substr(i);

            if (node->wildchild) {
                node = node->children[0];

                if (isPrefix(path, node->path) && node->type != RouterNodeType::CATCH_ALL
                    && (node->path.length() >= path.length() || path[node->path.length()] == '/')) {
                    continue;
                } else {
                    LOG_ERROR("wildcard conflict {}", fullPath);
                    return false;
                }
            }

            auto ch = path[0];

            // '/' after param
            if (node->type == RouterNodeType::PARAM && ch == '/' && node->children.size() == 1) {
                node = node->children[0];
                continue;
            }

            auto next = node->indice(ch);

            if (next) {
                node = next;
                continue;
            }

            if (ch != PARAM_CHAR) {
                node->indices.append(1, ch);
                auto child = new RawNode;
                child->type = RouterNodeType::STATIC;
                node->children.emplace_back(child);
                node = child;
            }
            return insertChild(node, path, fullPath, parser, ctrl);
        }

        if (node->handle) {
            LOG_ERROR("Handle to {} already exists.", fullPath);
            return false;
        } else {
            node->handle = ctrl;
            return true;
        }
    }
}

bool RawTree::insertChild(
    RawNode *node, string &path, const string &fullPath, PathParser &parser, HttpControllerPtr ctrl)
{
    while (true) {
        auto paramPos = findWildcard(path);
        if (paramPos == -1) {
            break;
        }

        assert(parser.params.size() >= 1);
        auto wild = move(parser.params.front());
        parser.params.pop();

        if (node->children.size() > 0) {
            LOG_ERROR("Wildcard segment {} conflicts with existing child in path {}", wild.pattern, fullPath);
            return false;
        }

        if (wild.type == RouterNodeType::PARAM) {
            if (paramPos > 0) {
                node->path = path.substr(0, paramPos);
                path = path.substr(paramPos);
                node->type = RouterNodeType::STATIC;
            } else {
                node->type = wild.type;
            }

            auto child = new RawNode;
            child->type = wild.type;
            child->re = move(wild.re);
            child->path = PARAM_CHAR;
            child->name = move(wild.name);

            node->wildchild = true;
            node->children.emplace_back(child);
            node = child;

            if (static_cast<size_t>(1) < path.length()) {
                path = path.substr(1);
                auto nChild = new RawNode;
                nChild->type = RouterNodeType::UNKNOWN;
                node->children.emplace_back(nChild);
                nChild->indices = path[0];
                node = nChild;
                continue;
            }

            node->handle = ctrl;
            return true;
        } else if (wild.type == RouterNodeType::CATCH_ALL) {
            if (paramPos > 0) {
                node->path = path.substr(0, paramPos);
                path = path.substr(paramPos);
                node->type = RouterNodeType::STATIC;
            } else {
                node->type = wild.type;
            }

            auto child = new RawNode;
            child->type = wild.type;

            node->children.emplace_back(child);
            node->wildchild = true;

            node = child;
            node->handle = ctrl;
            node->name = move(wild.name);

            return true;
        } else {
            assert(false);
        }
    }
    // no param was found. just insert it

    node->path = move(path);
    node->handle = ctrl;
    node->type = RouterNodeType::STATIC;
    node->indices.clear();
    return true;
}

void RawBuilder::sort()
{
    auto dispatchMethod = [](HttpMethod mtd) -> int {
        switch (mtd) {
            case HttpMethod::GET:
                return 0;
            case HttpMethod::POST:
                return 1;
            case HttpMethod::DELETE:
                return 2;
            case HttpMethod::PUT:
                return 3;
            case HttpMethod::OPTIONS:
                return 4;
            default:
                return 5;
        }
    };

    std::sort(trees_.begin(), trees_.end(), [&dispatchMethod](const MethodTree &first, const MethodTree &second) {
        auto mtd1 = get<0>(first);
        auto mtd2 = get<0>(second);
        return dispatchMethod(mtd1) < dispatchMethod(mtd2);
    });
}

RouterTrees::~RouterTrees()
{
    if (root_ != nullptr) {
        delete root_;
    }
}

FLATTEN
HttpController *Router::forward(HttpRequest *req, map<string, string> &params, bool &tsr) const
{
#ifndef NDEBUG
#define TSR_CHECK                                                                                         \
    do {                                                                                                  \
        LOG_DEBUG(                                                                                        \
            "TSR check, incoming Request path: '{}', now checking path: '{}', node path '{}', node type " \
            "{}, result: {}",                                                                             \
            req->getPath(),                                                                               \
            path,                                                                                         \
            prefix,                                                                                       \
            node->type(),                                                                                 \
            tsr);                                                                                         \
    } while (false);
#else
#define TSR_CHECK
#endif

    tsr = false;

    for (const auto &t : trees_) {
        if (t.mtd_ == req->getMethod()) {
            auto node = t.root_;
            auto path = req->getPathString();
            while (true) {
                bool continueWhile = false;
                auto &prefix = node->path();

                if (likely(path.size() > prefix.size())) {
                    if (likely(isPrefix(path, prefix))) {
                        path = path.substr(prefix.size());

                        if (likely(!node->wild())) {
                            char ch = path[0];

                            auto chindice = node->indice((ch));

                            if (likely(chindice != nullptr)) {
                                node = chindice;
                                continue;
                            }

                            if (path == "/" && node->handle()) {
                                tsr = true;

                                TSR_CHECK
                            }
                            return nullptr;
                        }
                        // handle wildcard child

                        node = node->children(0);

                        if (unlikely(node == nullptr)) {
                            return nullptr;
                        }

                        switch (node->type()) {
                            LIKELY case RouterNodeType::PARAM:
                            {
                                auto slash = findUntilSlash(path);
                                auto start = path.c_str();
                                auto end = start + slash;
                                if (unlikely(start == end)) {
                                    if (node->regex().match(empty)) {
                                        params.emplace(node->name(), string(start, end));
                                    }
                                } else {
                                    if (node->regex().match(start, end)) {
                                        params.emplace(node->name(), string(start, end));
                                    } else {
                                        return nullptr;
                                    }
                                }

                                {
                                    if (slash == 0 && node->handle()) {
                                        return node->handle();
                                    }

                                    if (slash < path.size()) {
                                        if (node->childrenCount() > 0) {
                                            path = path.substr(slash);

                                            node = node->children(0);

                                            continueWhile = true;
                                        }

                                        if (!continueWhile) {
                                            tsr = (path.size() == slash + 1);

                                            TSR_CHECK
                                            return nullptr;
                                        } else {
                                            continue;
                                        }
                                    } else {
                                        if (node->handle()) {
                                            return node->handle();
                                        } else {
                                            node = node->children(0);
                                            tsr = (node
                                                   && ((node->path() == "/" && node->handle())
                                                       || (node->indice() == "/")));

                                            TSR_CHECK
                                            return nullptr;
                                        }
                                    }
                                }
                            }
                            break;
                            LIKELY case RouterNodeType::CATCH_ALL:
                            {
                                assert(node->handle());

                                params.emplace(node->name(), move(path));
                                return node->handle();
                            }
                            break;
                            case RouterNodeType::STATIC:
                            case RouterNodeType::UNKNOWN:
                            case RouterNodeType::ROOT:
                            default:
                                LOG_DEBUG("un-expect RouterTreeNode: {}", node->type());
                                continue;
                        }
                    }
                } else if (prefix == path) {
                    auto &h = node->handle();

                    // We should have reached the node containing the handle.
                    // Check if this node has a handle registered
                    if (likely(h)) {
                        return h;
                    }

                    if (unlikely(node->wild())) {
                        auto fc = node->children(0);
                        assert(fc);

                        auto type = fc->type();
                        if (type == RouterNodeType::CATCH_ALL) {
                            params.emplace(fc->name(), empty);
                            return fc->handle();
                        } else if (type == RouterNodeType::PARAM) {
                            if (fc->regex().match(empty)) {
                                params.emplace(fc->name(), empty);
                                return fc->handle();
                            }
                        }
                    }

                    if (path == "/" && node->wild() && node->type() != RouterNodeType::ROOT) {
                        tsr = true;
                        TSR_CHECK

                        return h;
                    }

                    auto slash = node->indice('/');
                    if (slash != nullptr) {
                        node = slash;

                        tsr = (node->path().length() == 1 && node->handle());

                        if (!tsr && node->wild()) {
                            auto c = node->children(0);
                            assert(c);

                            tsr |= c->type() == RouterNodeType::CATCH_ALL;
                            tsr |= c->type() == RouterNodeType::PARAM && c->handle() && c->regex().match(empty);
                        }
                        TSR_CHECK
                    }

                    return h;
                }

                tsr = (path == "/")
                      || (prefix.length() == path.length() + 1 && prefix[path.length()] == '/' && node->handle())
                      || (node->children(0) && node->children(0)->type() == RouterNodeType::CATCH_ALL);

                TSR_CHECK
                return nullptr;
            }
        }
    }

    // here comes that incoming method not found in route tree
    return nullptr;

#undef TSR_CHECK
}

bool Router::buildTree(const string &prefix, RouterBuilder &&builder)
{
    RawBuilder raw;

    //decltype(builder.routes) simples;
    //simples.swap(builder.routes);
    auto &simples = builder.routes;
    auto success = true;

    for (auto simple = simples.begin(); simple != simples.end(); ++simple) {
        auto &item = *simple;
        auto &sc = get<2>(item);
        auto method = get<0>(item);
        auto path = get<1>(item);

        if (likely(raw.insert(prefix + path, method, sc))) {
            controllers.emplace_back(sc);
        } else {
            //            simples.erase(simple);
            success = false;
        }
    }

    builder.routes.clear();

    raw.sort();
    for (auto &rt : raw.trees_) {
        auto build = get<1>(rt).root.build();
        std::get<1>(rt).root.type = RouterNodeType::ROOT;
        this->trees_.emplace_back(get<0>(rt), move(build));
    }
    return success;
}

void RouterBuilder::add(HttpMethod mtd, const std::string &path, HttpControllerPtr ctrl)
{
    routes.emplace_back(mtd, path, ctrl);
}

HttpController *RouterBuilder::add(HttpMethod mtd, const std::string &path, SimpleControllerFunction &&cb)
{
    return add<SimpleControllerWrapper>(mtd, path, move(cb));
}

/*
HttpController *RouterBuilder::add(HttpMethod mtd, const char *path, SimpleControllerFunction &&cb)
{
    return add<SimpleControllerWrapper>(mtd, std::string(path), move(cb));
}*/

void RouterBuilder::add(const std::string &prefix, RouterBuilder &subRoutes)
{
    auto &re = getRegex();
    if (unlikely(!re.subRoute.match(prefix))) {
        LOG_ERROR("Bad Sub Route Prefix. Prefix must match '{}'", URL_SUBROUTE_PREFIX_PATTERN);
        return;
    }

    for (auto &sub : subRoutes.routes) {
        auto mtd = std::get<0>(sub);
        auto path = std::get<1>(sub);
        auto ctrl = std::get<2>(sub);
        add(mtd, prefix + path, ctrl);
    }
}

RouterBuilder::~RouterBuilder() {}
