#include "http_app.h"
#include "test.h"

class FormCtrl : public FormDataProcessorController
{
  public:
    ShaCalc file;
    ShaCalc random;
    std::map<std::string, std::string> value;

    std::map<std::string, std::string> ct;
    std::map<std::string, std::string> te;

    FormCtrl() = default;
    virtual ~FormCtrl() {}

    MOCK_METHOD((void), onRequest, (HttpRequestPtr, HttpResponsePtr&), (override));

    virtual bool onData(const std::string& keyName,
                        const std::string& fileName,
                        const std::string& contentType,
                        const std::string& transferEncoding,
                        const void* data,
                        MAYBE_UNUSED size_t offset,
                        size_t size,
                        bool finish) override
    {
        if (finish) {
            file.final();
            random.final();
        } else {
            ct[keyName] = contentType;
            te[keyName] = transferEncoding;

            if (keyName == "random") {
                random.update(data, size);
            } else if (keyName == "file" && fileName == "unittest") {
                file.update(data, size);
            } else {
                value.emplace(keyName, std::string(reinterpret_cast<const char*>(data), size));
            }
        }

        return true;
    }
};

TEST_F(HttpApp, formData)
{
    auto mock = add<FormCtrl>(HttpMethod::POST, myName);

    DEFAULT_MOCK_REQUEST_WITH_PARAMTER(mock, (k201Created, myName));
    DEFAULT_MOCK_REQUEST_TIMES(mock, 1);

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