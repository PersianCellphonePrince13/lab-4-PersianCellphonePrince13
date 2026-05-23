#include "lab4/resource.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

using namespace lab4::resource;

void cleanup_test_files()
{
    std::remove("test_raii.txt");
    std::remove("test_move.txt");
    std::remove("test_shared.txt");
    std::remove("test_cache.txt");
    std::remove("test_rw.txt");
    std::remove("test_errors.txt");
    std::remove("test1.txt");
    std::remove("test2.txt");
    std::remove("test3.txt");
    std::remove("test_release.txt");
    std::remove("test_multi.txt");
}

TEST_CASE("ResourceError is thrown when opening non-existent file", "[ResourceError]")
{
    try {
        FileHandle file("");
        REQUIRE(false);
    } catch (const ResourceError&) {
        REQUIRE(true);
    }
}

TEST_CASE("RAII automatically closes file on destruction", "[RAII]")
{
    {
        FileHandle file("test_raii.txt");
        file.write("RAII test data");
        REQUIRE(file.is_open());
    }

    FileHandle file2("test_raii.txt");
    REQUIRE(file2.is_open());
}

TEST_CASE("Move semantics transfers ownership correctly", "[Move]")
{
    FileHandle file1("test_move.txt");
    file1.write("Moving ownership test");

    FileHandle file2(std::move(file1));
    REQUIRE_FALSE(file1.is_open());
    REQUIRE(file2.is_open());

    std::string content = file2.read();
    REQUIRE(content.find("Moving ownership test") != std::string::npos);
}

TEST_CASE("ResourceManager provides shared ownership", "[SharedOwnership]")
{
    ResourceManager manager;

    auto resource1 = manager.acquire("test_shared.txt");
    auto resource2 = manager.acquire("test_shared.txt");

    resource1->write("Shared content test\n");

    REQUIRE(resource1->read() == resource2->read());
    REQUIRE(manager.cache_size() == 1);

    resource1.reset();
    REQUIRE(manager.cache_size() == 1);
    REQUIRE(resource2->is_open());
}

TEST_CASE("ResourceManager cache manages resource lifetime", "[Cache]")
{
    ResourceManager manager;

    SECTION("Cache returns same resource for same filename")
    {
        auto file1 = manager.acquire("test_cache.txt");
        auto file2 = manager.acquire("test_cache.txt");
        REQUIRE(manager.cache_size() == 1);
        REQUIRE(file1.get() == file2.get());
    }

    SECTION("Cache releases expired resources on cleanup")
    {
        auto file1 = manager.acquire("test_cache.txt");
        file1->write("First write\n");
        REQUIRE(manager.cache_size() == 1);

        auto file2 = manager.acquire("test_cache.txt");
        REQUIRE(manager.cache_size() == 1);

        file1.reset();
        file2.reset();

        manager.cleanup();
        REQUIRE(manager.cache_size() == 0);
    }
}

TEST_CASE("File read/write operations work correctly", "[IO]")
{
    FileHandle file("test_rw.txt");

    file.write("Line 1\n");
    file.write("Line 2\n");
    file.write("Line 3\n");

    std::string content = file.read();
    REQUIRE(content.find("Line 1") != std::string::npos);
    REQUIRE(content.find("Line 2") != std::string::npos);
    REQUIRE(content.find("Line 3") != std::string::npos);
}

TEST_CASE("Operations on closed file throw ResourceError", "[ErrorHandling]")
{
    ResourceManager manager;
    auto file = manager.acquire("test_errors.txt");
    file->write("Initial data");
    file->close();

    REQUIRE_THROWS_AS(file->write("This should fail"), ResourceError);
    REQUIRE_THROWS_AS(file->read(), ResourceError);
}

TEST_CASE("Release and cleanup methods work correctly", "[Manager]")
{
    ResourceManager manager;

    SECTION("Release removes resource from cache")
    {
        auto file = manager.acquire("test_release.txt");
        REQUIRE(manager.cache_size() == 1);

        manager.release("test_release.txt");
        REQUIRE(manager.cache_size() == 0);
    }

    SECTION("Cleanup removes only expired resources")
    {
        auto f1 = manager.acquire("test1.txt");
        auto f2 = manager.acquire("test2.txt");
        auto f3 = manager.acquire("test3.txt");

        REQUIRE(manager.cache_size() == 3);

        f1.reset();
        f2.reset();

        manager.cleanup();
        REQUIRE(manager.active_resources() == 1);
    }
}

TEST_CASE("Multiple acquires return same shared_ptr", "[SharedPtr]")
{
    ResourceManager manager;

    auto ptr1 = manager.acquire("test_multi.txt");
    auto ptr2 = manager.acquire("test_multi.txt");
    auto ptr3 = manager.acquire("test_multi.txt");

    REQUIRE(ptr1.get() == ptr2.get());
    REQUIRE(ptr2.get() == ptr3.get());
    REQUIRE(manager.cache_size() == 1);

    ptr1->write("Multiple access test\n");
    REQUIRE(ptr2->read() == ptr3->read());
}

struct GlobalCleanup
{
    ~GlobalCleanup()
    {
        cleanup_test_files();
    }
};

GlobalCleanup cleanup;

