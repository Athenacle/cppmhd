#include <cppmhd/app.h>
#include <cppmhd/controller.h>

using namespace cppmhd;

int main()
{
    App app("10.70.20.160", 5432);

    app.add(HttpMethod::GET, "/hello/{:name}", [](HttpRequestPtr req) -> HttpResponsePtr {
        auto resp = std::make_shared<HttpResponse>();
        auto msg = "hello! Your name is " + req->getParam("name");
        resp->status(k200OK);
        resp->body(msg);
        return resp;
    });


    app.add(HttpMethod::GET, "/world/", [](HttpRequestPtr) -> HttpResponsePtr {
        auto resp = std::make_shared<HttpResponse>();
        const char hw[] = "This is the world!\n";
        resp->status(k200OK);
        resp->body(sizeof(hw) - 1, hw);
        return resp;
    });

    // app.host() = "my-host";

    app.start();

    return 0;
}