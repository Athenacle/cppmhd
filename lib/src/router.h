#ifndef CPPMHD_INTERNAL_ROUTER_H_
#define CPPMHD_INTERNAL_ROUTER_H_

#include "config.h"

#include <cppmhd/router.h>

#include "utils.h"

#define NORMAL_URL_CHAR "[\\w\\.\\-_]"

#define URL_STATIC_PART_PATTERN "^" NORMAL_URL_CHAR "+$"
#define URL_MATCHALL_PATTERN "^(?<prefix>" NORMAL_URL_CHAR "*){\\*{2}(:(?<name>\\w+))?}$"

#define URL_REGEX_PATTERN "^(?<prefix>" NORMAL_URL_CHAR "*){(?<pattern>[^:]*)(:(?<name>\\w+))?}$"

#define URL_REGEX_DEFAULT_PATTERN NORMAL_URL_CHAR "*"

#define URL_ROUTE_PATTERN "(^(/[^/]+)+/?$)|(^/$)"

#define URL_SUBROUTE_PREFIX_PATTERN "^(/[^/]+)+$"

CPPMHD_NAMESPACE_BEGIN

struct Handler {
    HttpController* controller;
    HttpControllerPtr ptr;

    Handler(HttpControllerPtr ctrl) : ptr(ctrl)
    {
        controller = ptr.get();
    }
    Handler() : Handler(nullptr) {}

    Handler(const Handler&) = delete;

    const HttpController* ctrl() const
    {
        return controller;
    }

    Handler& operator=(Handler&& h)
    {
        controller = h.controller;
        h.controller = nullptr;
        return *this;
    }

    operator HttpController*()
    {
        return controller;
    }

    void swap(Handler& other)
    {
        std::swap(controller, other.controller);
    }

    operator bool() const
    {
        return controller != nullptr;
    }
};

enum class RouterNodeType {
    UNKNOWN,

    ROOT,
    STATIC,
    PARAM,
    CATCH_ALL
};

class RouterNode
{
    std::string path_;
    std::string indices_;
    RouterNode** children_;

    Handler handle_;
    RouterNodeType type_;
    Regex re_;
    std::string name_;

    bool wildchild_{false};

  public:
    RouterNode()
    {
        type_ = RouterNodeType::UNKNOWN;
    }

    RouterNode(const std::string& p,
               const std::string& i,
               RouterNode** ch,
               Handler&& h,
               RouterNodeType t,
               Regex&& re,
               const std::string& name,
               bool wild)
        : path_(p), indices_(i), children_(ch), type_(t), re_(std::move(re)), name_(name), wildchild_(wild)
    {
        handle_ = std::move(h);
    }

    RouterNode& operator=(RouterNode&&);

    ~RouterNode()
    {
        if (children_) {
            for (auto i = 0u; children_[i]; i++) {
                delete children_[i];
            }

            delete[] children_;
        }
    }
    const std::string& name() const
    {
        return name_;
    }
    const Regex& regex() const
    {
        return re_;
    }
    bool wild() const
    {
        return wildchild_;
    }
    RouterNodeType type() const
    {
        return type_;
    }

    Handler& handle()
    {
        return handle_;
    }

    const std::string& indice() const
    {
        return indices_;
    }

    RouterNode* indice(char c) const
    {
        if (likely(children_ != nullptr)) {
            for (auto i = 0u; i < indices_.length(); i++) {
                if (indices_[i] == c) {
                    return children_[i];
                }
            }
        }
        return nullptr;
    }

    RouterNode* children(int pos) const
    {
        if (children_ == nullptr) {
            return nullptr;
        } else {
            for (int i = 0; i < pos; i++) {
                if (children_[i] == nullptr) {
                    return nullptr;
                }
            }
            return children_[pos];
        }
    }

    size_t childrenCount() const
    {
        int i = 0u;
        if (children_ != nullptr) {
            for (auto p = children_; *p; p++, i++) {
            }
        }
        return i;
    }

    const std::string& path() const
    {
        return path_;
    }
};

class RouterTrees
{
    friend class Router;

    HttpMethod mtd;
    RouterNode* root;

  public:
    RouterTrees(RouterTrees&& other)
    {
        mtd = other.mtd;
        root = other.root;
        other.root = nullptr;
    }
    RouterTrees(HttpMethod mtd, RouterNode* root) : mtd(mtd)
    {
        this->root = root;
    }

    ~RouterTrees();

    const RouterNode* getTree() const
    {
        return root;
    }
    RouterNode* getTree()
    {
        return root;
    }
    HttpMethod getHttpMethod() const
    {
        return mtd;
    }
};

class Router
{
    std::vector<HttpControllerPtr> controllers;

    std::vector<RouterTrees> trees_;

    bool buildTree(const std::string& prefix, RouterBuilder&&);

    RouterTrees& getMethodTree(HttpMethod mth);

    bool valid;

  public:
    ~Router() {}

    Router(Router&& other)
    {
        std::swap(trees_, other.trees_);
        std::swap(valid, other.valid);
        std::swap(controllers, other.controllers);
    }

    Router(RouterBuilder&& rb, const std::string prefix)
    {
        valid = buildTree(prefix, std::move(rb));
    }

    explicit Router(RouterBuilder&& builder) : Router(std::move(builder), "") {}

    Router(const Router&) = delete;

    bool build()
    {
        return valid;
    }

    HttpController* forward(HttpRequest*, std::map<std::string, std::string>&, bool&) const;
};

CPPMHD_NAMESPACE_END

#endif