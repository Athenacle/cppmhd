#include "http_app.h"

#define FORMAT_INETADDRESS
#include "format.h"

TEST_F(HttpApp, basicParam)
{
    auto fn = [](const std::string &name, const std::string &id) {
        return fmt::format("hello {}!, Your id is {}.", name, id);
    };

    app->add(HttpMethod::GET, "/root/{\\w+:name}/{\\d+:id}", [&fn](HttpRequestPtr req) -> HttpResponsePtr {
        auto name = req->getParam("name");
        auto id = req->getParam("id");
        auto other = req->getParam("other");

        EXPECT_EQ(other, "");

        auto resp = std::make_shared<HttpResponse>();
        resp->body(fn(name, id));
        resp->status(k200OK);
        return resp;
    });

    start();

    std::string n = "naaaaame";
    int id = 123456;
    Curl c = curl(fmt::format("/root/{}/{}", n, id));
    c.perform();
    ASSERT_EQ(c.status(), k200OK);
    ASSERT_EQ(c.body(), fn(n, fmt::to_string(id)));
}

TEST_F(HttpApp, notFound)
{
    auto mock = add<TestCtrl>(HttpMethod::GET, "/some-strange-url");
    start();

    DEFAULT_MOCK_REQUEST(mock);
    DEFAULT_MOCK_CONNECTION(mock);
    DEFAULT_MOCK_REQUEST_TIMES(mock, 0);
    DEFAULT_MOCK_CONNECTION_TIMES(mock, 0);

    Curl c(host, port, myName);
    c.perform();
    EXPECT_EQ(c.status(), k404NotFound);
}

TEST_F(HttpApp, simpleHostCheck)
{
    const std::string hostString = "TestHost";

    auto mock = add<TestCtrl>(HttpMethod::GET, myName);
    app->host() = hostString;
    start();

    DEFAULT_MOCK_REQUEST(mock);
    DEFAULT_MOCK_CONNECTION(mock);
    DEFAULT_MOCK_REQUEST_TIMES(mock, 1);
    DEFAULT_MOCK_CONNECTION_TIMES(mock, 1);

    Curl c(host, port, myName);
    c.perform();
    ASSERT_EQ(c.status(), k400BadRequest);

    Curl cc(host, port, myName);
    cc.addRequestHeader("host", hostString);
    cc.perform();
    ASSERT_EQ(cc.status(), k200OK);
}

TEST_F(HttpApp, rejectNotHttp11)
{
    auto mock = add<TestCtrl>(HttpMethod::GET, myName);
    start();

    Curl h10(host, port, myName);
    h10.setup(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    h10.perform();

    DEFAULT_MOCK_REQUEST(mock);
    DEFAULT_MOCK_CONNECTION(mock);
    DEFAULT_MOCK_REQUEST_TIMES(mock, 1);
    DEFAULT_MOCK_CONNECTION_TIMES(mock, 1);


    EXPECT_EQ(h10.status(), k505HttpVersionNotSupported);

    Curl h11(host, port, myName);
    h11.setup(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    h11.perform();

    EXPECT_EQ(h11.status(), k200OK);
}

TEST_F(HttpApp, simpleTsr)
{
    auto mock = add<TestCtrl>(HttpMethod::GET, myName + "/");
    start();

    DEFAULT_MOCK_REQUEST(mock);
    DEFAULT_MOCK_CONNECTION(mock);
    DEFAULT_MOCK_REQUEST_TIMES(mock, 1);
    DEFAULT_MOCK_CONNECTION_TIMES(mock, 1);

    Curl h1(host, port, myName);
    h1.perform();
    EXPECT_EQ(h1.status(), k301MovePermanently);
    EXPECT_EQ(h1.headers()["Location"], myName + "/");

    Curl h2(host, port, myName);
    h2.followLocation();
    h2.perform();
    EXPECT_EQ(h2.status(), k200OK);
}

TEST_F(HttpApp, badMethod)
{
    add<TestCtrl>(HttpMethod::GET, myName);
    start();


    Curl h1(host, port, myName);
    h1.setup(CURLOPT_CUSTOMREQUEST, "SOME-BAD-METHOD");
    h1.perform();
    EXPECT_EQ(h1.status(), k405MethodNotAllowed);
}

TEST_F(HttpApp, bodyInGet)
{
    auto mock = add<TestCtrl>(HttpMethod::GET, myName);
    start();
    InetAddress addr;

    DEFAULT_MOCK_CONNECTION(mock);
    DEFAULT_MOCK_CONNECTION_TIMES(mock, 1);

    ASSERT_TRUE(InetAddress::parse(addr, host, port));

    char req[] =
        "GET /bodyInGet HTTP/1.1\r\n"  // this path should match my test name
        "user-agent: some-fake-user-agent/0\r\n"
        "accept: */*\r\n"
        "content-type: text/plain\r\n"
        "content-length: 5\r\n"
        "accept-encoding: gzip, deflate, br\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello";

#ifdef ON_WINDOWS
    WSADATA wsaData = {0};
    ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsaData), 0)
        << fmt::format("WSAStartup failed. WSAGetLastError: {}", WSAGetLastError());
#endif

    auto sock = socket(AF_INET, SOCK_STREAM, 0);

#ifdef ON_WINDOWS
    ASSERT_NE(sock, INVALID_SOCKET) << fmt::format(
        "socket(AF_INET, SOCK_STREAM,0) failed: {}, WsGetLastError: {}", sock, WSAGetLastError());
#else
    ASSERT_GT(sock, 0) << fmt::format("socket(AF_INET, SOCK_STREAM, 0) failed: {}", strerror(errno));
#endif

    ASSERT_EQ(connect(sock, addr.getSocket(), addr.socketLen()), 0)
        << fmt::format("connect to {:a} failed: {}", addr, strerror(errno));

    ASSERT_EQ(sizeof(req) - 1, send(sock, req, sizeof(req) - 1, 0)) << fmt::format("send failed: {}", strerror(errno));

    const size_t bufsize = 1024;
    char buffer[bufsize];

    auto r = recv(sock, buffer, bufsize, MSG_WAITALL);
    ASSERT_GT(r, 0) << fmt::format("recv failed: {}", strerror(errno));
    ASSERT_LT(r, bufsize) << "received data too large.";

#ifdef ON_WINDOWS
    ASSERT_NE(closesocket(sock), SOCKET_ERROR)
        << fmt::format("closesocket failed. WSGetLastError: {}", WSAGetLastError());
#else
    close(sock);
#endif

    Regex output;
    ASSERT_TRUE(Regex::compileRegex(output, "^HTTP/1.1 400 Bad Request"));
    ASSERT_TRUE(output.match(buffer, r));
}