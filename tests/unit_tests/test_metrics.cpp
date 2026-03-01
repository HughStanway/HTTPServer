#include <gtest/gtest.h>
#include <httpserver/metrics.h>
#include <chrono>
#include <thread>

using namespace HTTPServer;

TEST(MetricsTests, ActiveConnections) {
    auto initial = Metrics::instance().snapshot().d_activeConnections;
    Metrics::instance().incrementActiveConnection();
    EXPECT_EQ(Metrics::instance().snapshot().d_activeConnections, initial + 1);
    Metrics::instance().decrementActiveConnection();
    EXPECT_EQ(Metrics::instance().snapshot().d_activeConnections, initial);
}

TEST(MetricsTests, TotalRequests) {
    auto initial = Metrics::instance().snapshot().d_totalRequests;
    Metrics::instance().incrementTotalRequests();
    EXPECT_EQ(Metrics::instance().snapshot().d_totalRequests, initial + 1);
}

TEST(MetricsTests, ResponseStatus) {
    auto s1 = Metrics::instance().snapshot();
    Metrics::instance().recordResponseStatus(StatusCode::OK);           // 2xx
    Metrics::instance().recordResponseStatus(StatusCode::BadRequest);   // 4xx
    Metrics::instance().recordResponseStatus(StatusCode::InternalServerError); // 5xx
    Metrics::instance().recordResponseStatus(StatusCode::MovedPermanently); // 3xx

    auto s2 = Metrics::instance().snapshot();
    EXPECT_EQ(s2.d_responses2xx, s1.d_responses2xx + 1);
    EXPECT_EQ(s2.d_responses3xx, s1.d_responses3xx + 1);
    EXPECT_EQ(s2.d_responses4xx, s1.d_responses4xx + 1);
    EXPECT_EQ(s2.d_responses5xx, s1.d_responses5xx + 1);
}

TEST(MetricsTests, BytesTracking) {
    auto s1 = Metrics::instance().snapshot();
    Metrics::instance().recordBytesReceived(100);
    Metrics::instance().recordBytesSent(200);

    auto s2 = Metrics::instance().snapshot();
    EXPECT_EQ(s2.d_totalBytesReceived, s1.d_totalBytesReceived + 100);
    EXPECT_EQ(s2.d_totalBytesSent, s1.d_totalBytesSent + 200);
}

TEST(MetricsTests, ProcessingTime) {
    auto s1 = Metrics::instance().snapshot();
    Metrics::instance().recordRequestProcessingTime(std::chrono::milliseconds(50));

    auto s2 = Metrics::instance().snapshot();
    EXPECT_EQ(s2.d_totalRequestProcessingTimeMs, s1.d_totalRequestProcessingTimeMs + 50);
}
