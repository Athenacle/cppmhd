#include "http_app.h"
#include "test.h"


class FormCtrl : public FormDataProcessorController
{
  public:
    ShaCalc file;
    ShaCalc random;
    void* ptr;
    std::map<std::string, std::string> value;

    std::map<std::string, std::string> ct;
    std::map<std::string, std::string> te;

    FormCtrl(void* p)
    {
        ptr = p;
    }
    virtual ~FormCtrl() {}

    virtual void postConnection(HttpRequestPtr req) override
    {
        req->userdata(ptr);
    }

    MOCK_METHOD((void), onRequest, (HttpRequestPtr, HttpResponsePtr&), (override));

    virtual bool onData(HttpRequestPtr req,
                        const std::string& keyName,
                        const std::string& fileName,
                        const std::string& contentType,
                        const std::string& transferEncoding,
                        const void* data,
                        MAYBE_UNUSED size_t offset,
                        size_t size,
                        bool finish) override
    {
        EXPECT_EQ(req->userdata(), ptr);

        if (finish) {
            file.final();
            random.final();
        } else {
            ct[keyName] = contentType;
            te[keyName] = transferEncoding;

            if (keyName == "random") {
                random.update(data, size);
            } else if (keyName == "file" && fileName == MY_BINARY_FILENAME) {
                file.update(data, size);
            } else {
                value.emplace(keyName, std::string(reinterpret_cast<const char*>(data), size));
            }
        }

        return true;
    }
};


class FormSimpleRequestMock : public ActionInterface<void(HttpRequestPtr, HttpResponsePtr&)>
{
    HttpStatusCode code_;
    std::string body_;
    void* ptr;

  public:
    FormSimpleRequestMock() : FormSimpleRequestMock(k200OK) {}

    FormSimpleRequestMock(HttpStatusCode code) : FormSimpleRequestMock(code, "", nullptr) {}

    FormSimpleRequestMock(HttpStatusCode code, const std::string b, void* p) : code_(code), body_(b)
    {
        ptr = p;
    }

    ~FormSimpleRequestMock() = default;


    void Perform(const std::tuple<HttpRequestPtr, HttpResponsePtr&>& args)
    {
        auto& resp = std::get<1>(args);
        auto& req = std::get<0>(args);
        EXPECT_EQ(req->userdata(), ptr);
        resp = std::make_shared<HttpResponse>();
        if (body_.length() > 0) {
            resp->body("hello!");
            resp->header(CPPMHD_HTTP_HEADER_CONTENT_TYPE) = CPPMHD_HTTP_MIME_TEXT_PLAIN;
        }
        resp->status(code_);
    }
};

TEST_F(HttpApp, formData)
{
    auto mock = add<FormCtrl>(HttpMethod::POST, myName, this);


    DEFAULT_MOCK_REQUEST_TIMES(mock, 1);
    ON_CALL(*mock, MOCK_CTRL(onRequest))
        .WillByDefault(MakeAction(new FormSimpleRequestMock(k201Created, myName, this)));

    start();
    auto size = 4096 + rand() % 4096;
    auto block = generateRandom(size);

    auto c = curl();
    std::map<std::string, std::string> data = {{"a", "10"}, {"b", "bb"}, {"c", "cc"}};
    c.mime_upload(data);
    c.mime_upload("random", block, size);

    ShaCalc random, file;

    random.update(block, size);
    random.final();

#if defined MY_BINARY_FILE && defined MY_BINARY_FILENAME
    c.mime_uploadfile(MY_BINARY_FILENAME, MY_BINARY_FILE);
    ASSERT_TRUE(calcFile(file, MY_BINARY_FILE));
#endif

    c.perform();
    ASSERT_EQ(c.status(), k201Created);
    ASSERT_EQ(mock->value, data);

    ASSERT_TRUE(mock->random);
    ASSERT_TRUE(mock->random == random);

#if defined MY_BINARY_FILE && defined MY_BINARY_FILENAME
    ASSERT_TRUE(mock->file);
    ASSERT_TRUE(mock->file == file) << fmt::format("in: {}, actual: {}", mock->file.format(), file.format());
#endif

    delete[] block;
}