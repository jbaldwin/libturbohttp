#include <liquid/Liquid.h>

#include <iostream>
#include <chrono>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // theres no transfer encoding so this is safe to re-use as input
    std::string buffer = "GET /cookies HTTP/1.1\r\nHost: 127.0.0.1:8090\r\nConnection: keep-alive\r\nCache-Control: max-age=0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.17 (KHTML, like Gecko) Chrome/24.0.1312.56 Safari/537.17\r\nAccept-Encoding: gzip,deflate,sdch\r\nAccept-Language: en-US,en;q=0.8\r\nAccept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\r\nCookie: name=wookie\r\n\r\n";
    constexpr size_t iterations = 1'000'000;

    auto start = std::chrono::steady_clock::now();
    for(size_t i = 0; i < iterations; ++i)
    {
        liquid::request::Request request{};
        request.Parse(buffer);
    }
    auto end = std::chrono::steady_clock::now();

    auto total = end - start;
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total).count();
    std::cout << "Total running time in ms: " << total_ms  << "\n";

    double requests_per_second = ((double)iterations) / total_ms * 1000;
    std::cout << "Requests/sec: " << (uint64_t)requests_per_second << "\n";
    std::cout << "MegaBytes per second: " << (buffer.length() * requests_per_second) / 1024 / 1024 << "\n";

    return 0;
}