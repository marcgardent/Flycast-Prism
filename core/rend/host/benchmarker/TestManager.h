#pragma once
#include <vector>
#include <memory>
#include <string>
#include "TestCase.h"

class TestManager {
public:
    static TestManager& instance() {
        static TestManager inst;
        return inst;
    }

    void registerTest(std::unique_ptr<TestCase> test) {
        tests.push_back(std::move(test));
    }

    const std::vector<std::unique_ptr<TestCase>>& getTests() const {
        return tests;
    }

    TestCase* getTestById(const std::string& id) const {
        for (auto& t : tests) {
            if (t->getId() == id) return t.get();
        }
        return nullptr;
    }

private:
    TestManager() = default;
    std::vector<std::unique_ptr<TestCase>> tests;
};
