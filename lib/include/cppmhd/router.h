#ifndef CPPMHD_ROUTER_H_
#define CPPMHD_ROUTER_H_

#include <cppmhd/controller.h>
#include <cppmhd/core.h>

#include <functional>
#include <utility>
#include <vector>

CPPMHD_NAMESPACE_BEGIN

using SimpleControllerFunction = std::function<HttpResponsePtr(HttpRequestPtr)>;

class RouterBuilder
{
    friend class Router;

    std::vector<std::tuple<HttpMethod, std::string, HttpControllerPtr>> routes;

    void *getTree(HttpMethod mth);

    void add(HttpMethod mtd, const std::string &path, HttpControllerPtr ctrl);

    RouterBuilder &operator=(const RouterBuilder &) = delete;
    RouterBuilder &operator=(RouterBuilder &&) = delete;
    RouterBuilder(RouterBuilder &&) = delete;
    RouterBuilder(const RouterBuilder &) = delete;

  public:
    ~RouterBuilder();

    RouterBuilder() = default;

    void add(const std::string &prefix, RouterBuilder &subRoutes);

    template <class CTRL, class... Args>
    CTRL *add(HttpMethod mtd, const std::string &path, Args &&...args)
    {
        auto ctrl = cppmhd_construct<CTRL>(std::forward<Args>(args)...);
        auto ptr = std::shared_ptr<HttpController>(ctrl, cppmhd_delete<CTRL>());
        add(mtd, path, ptr);
        return ctrl;
    }

    HttpController *add(HttpMethod mtd, const std::string &path, SimpleControllerFunction &&cb);
};

CPPMHD_NAMESPACE_END

#endif