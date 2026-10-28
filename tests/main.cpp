/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace axiom::test {

/// GoogleTest の各テスト実行時間をマイクロ秒（µs）単位で高精度に計測・出力するカスタムリスナー。
class MicrosecondPrinter : public testing::EmptyTestEventListener {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    void OnTestProgramStart(const testing::UnitTest& unit_test) override {
        program_start_ = Clock::now();
        std::cout << "[==========] Running " << unit_test.test_to_run_count()
                  << " tests from " << unit_test.test_suite_to_run_count()
                  << " test suites." << std::endl;
        std::cout << "[----------] Global test environment set-up." << std::endl;
    }

    void OnTestSuiteStart(const testing::TestSuite& test_suite) override {
        suite_start_ = Clock::now();
        std::cout << "[----------] " << test_suite.test_to_run_count()
                  << " tests from " << test_suite.name() << std::endl;
    }

    void OnTestStart(const testing::TestInfo& test_info) override {
        test_start_ = Clock::now();
        std::cout << "[ RUN      ] " << test_info.test_suite_name() << "." << test_info.name() << std::endl;
    }

    void OnTestPartResult(const testing::TestPartResult& result) override {
        if (result.failed()) {
            std::cerr << result.file_name() << ":" << result.line_number() << ": Failure\n"
                      << result.message() << std::endl;
        }
    }

    void OnTestEnd(const testing::TestInfo& test_info) override {
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(
            Clock::now() - test_start_).count();

        if (test_info.result()->Passed()) {
            std::cout << "[       OK ] " << test_info.test_suite_name() << "." << test_info.name()
                      << " (" << std::fixed << std::setprecision(1) << duration << " µs)" << std::endl;
        } else {
            std::cout << "[  FAILED  ] " << test_info.test_suite_name() << "." << test_info.name()
                      << " (" << std::fixed << std::setprecision(1) << duration << " µs)" << std::endl;
        }
    }

    void OnTestSuiteEnd(const testing::TestSuite& test_suite) override {
        auto suite_duration = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(
            Clock::now() - suite_start_).count();

        std::cout << "[----------] " << test_suite.test_to_run_count()
                  << " tests from " << test_suite.name() << " ("
                  << std::fixed << std::setprecision(1) << suite_duration << " µs total)" << std::endl;
        std::cout << std::endl;
    }

    void OnTestProgramEnd(const testing::UnitTest& unit_test) override {
        auto total_duration = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
            Clock::now() - program_start_).count();

        std::cout << "[----------] Global test environment tear-down" << std::endl;
        std::cout << "[==========] " << unit_test.test_to_run_count() << " tests ran. ("
                  << std::fixed << std::setprecision(2) << total_duration << " ms total)" << std::endl;
        std::cout << "[  PASSED  ] " << unit_test.successful_test_count() << " tests." << std::endl;

        if (unit_test.failed_test_count() > 0) {
            std::cout << "[  FAILED  ] " << unit_test.failed_test_count() << " tests." << std::endl;
        }
    }

private:
    TimePoint program_start_;
    TimePoint suite_start_;
    TimePoint test_start_;
};

} // namespace axiom::test

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);

    // デフォルトのミリ秒プリンターを外し、高精度マイクロ秒プリンターに差し替える
    auto& listeners = testing::UnitTest::GetInstance()->listeners();
    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new axiom::test::MicrosecondPrinter());

    return RUN_ALL_TESTS();
}
