/**
 * @file Logger.cpp
 * @brief 日志工具类实现 / Logger utility class implementation
 *
 * 实现双通道日志记录（文件+控制台）、断点续传、自动创建日志目录等功能。
 * Implements dual-channel logging (file + console), checkpoint/resume,
 * and automatic log directory creation.
 */

#include "Logger.h"
#include <iomanip>
#include <chrono>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// 单例模式 / Singleton pattern
// ============================================================================

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
}

Logger::~Logger()
{
    shutdown();
}

// ============================================================================
// 初始化与关闭 / Initialization and shutdown
// ============================================================================

void Logger::init(const std::string& logFilePath, Level minLevel)
{
    minLevel_ = minLevel;
    logFilePath_ = logFilePath;

    // 确保日志文件所在目录存在 / Ensure log file's parent directory exists
    if (!ensureLogDirectory(logFilePath))
    {
        initialized_ = false;
        std::cerr << "[Logger] Cannot create log directory for: " << logFilePath << std::endl;
        // 仍然尝试打开文件 / Still try to open the file
    }

    // 关闭已有文件 / Close existing file
    if (logFile_.is_open())
    {
        logFile_.close();
    }

    // 以追加模式打开日志文件 / Open log file in append mode
    logFile_.open(logFilePath, std::ios::app);
    if (logFile_.is_open())
    {
        initialized_ = true;
        log(INFO, "Logger initialized. Log file: " + logFilePath);

        // 写入分隔线，标记新的运行会话 / Write separator marking new run session
        log(INFO, "============================================================");
    }
    else
    {
        initialized_ = false;
        std::cerr << "[Logger] Failed to open log file: " << logFilePath << std::endl;
    }
}

void Logger::shutdown()
{
    if (logFile_.is_open())
    {
        log(INFO, "Logger shutting down.");
        logFile_.close();
    }
    initialized_ = false;
}

std::string Logger::getLogFilePath() const
{
    return logFilePath_;
}

bool Logger::ensureLogDirectory(const std::string& filePath)
{
    try
    {
        fs::path dir = fs::path(filePath).parent_path();
        if (!dir.empty() && !fs::exists(dir))
        {
            fs::create_directories(dir);
            return true;
        }
        return true;
    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "[Logger] Cannot create log directory: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// 日志写入 / Log writing
// ============================================================================

void Logger::log(Level level, const std::string& message)
{
    // 过滤低于最小级别的日志 / Filter logs below minimum level
    if (level < minLevel_) return;

    std::string timestamp = getTimestamp();
    std::string levelStr = levelToString(level);
    std::string line = "[" + timestamp + "] [" + levelStr + "] " + message;

    // 输出到控制台 / Output to console
    // WARN和ERROR输出到stderr / WARN and ERROR go to stderr
    if (level >= WARN)
    {
        std::cerr << line << std::endl;
    }
    else
    {
        std::cout << line << std::endl;
    }

    // 输出到文件 / Output to file
    if (initialized_ && logFile_.is_open())
    {
        logFile_ << line << std::endl;
        logFile_.flush();  // 立即刷新，确保断电后日志不丢失 / Flush immediately for crash safety
    }
}

void Logger::debug(const std::string& message)
{
    log(DEBUG, message);
}

void Logger::info(const std::string& message)
{
    log(INFO, message);
}

void Logger::warn(const std::string& message)
{
    log(WARN, message);
}

void Logger::error(const std::string& message)
{
    log(ERROR, message);
}

// ============================================================================
// 结果与进度记录 / Result and progress tracking
// ============================================================================

void Logger::result(const std::string& funcName, const std::string& key, double value)
{
    std::ostringstream oss;
    oss << "[RESULT] " << funcName << " | " << key << " = "
        << std::fixed << std::setprecision(6) << value;
    log(INFO, oss.str());
}

void Logger::progress(int step, int total, const std::string& message)
{
    std::ostringstream oss;
    oss << "[PROGRESS] Step " << step << "/" << total << " - " << message;
    log(INFO, oss.str());
}

// ============================================================================
// 断点续传 / Checkpoint for resume
// ============================================================================

void Logger::writeCheckpoint(int step, int total, const std::string& message)
{
    std::ostringstream oss;
    oss << "[CHECKPOINT] step=" << step << " total=" << total << " message=" << message;
    log(INFO, oss.str());
}

Logger::Checkpoint Logger::readLastCheckpoint()
{
    Checkpoint cp;
    cp.valid = false;

    // 若日志文件路径为空，直接返回 / Return if no log file path
    if (logFilePath_.empty()) return cp;

    // 打开日志文件读取 / Open log file for reading
    std::ifstream inFile(logFilePath_);
    if (!inFile.is_open()) return cp;

    std::string line;
    std::string lastCheckpointLine;

    // 逐行查找最后一个 [CHECKPOINT] 行 / Find last [CHECKPOINT] line
    while (std::getline(inFile, line))
    {
        if (line.find("[CHECKPOINT]") != std::string::npos)
        {
            lastCheckpointLine = line;
        }
    }
    inFile.close();

    // 若未找到断点，返回无效 / Return invalid if no checkpoint found
    if (lastCheckpointLine.empty()) return cp;

    // 解析断点行 / Parse checkpoint line
    // 格式: [timestamp] [INFO ] [CHECKPOINT] step=X total=Y message=...
    try
    {
        size_t stepPos = lastCheckpointLine.find("step=");
        size_t totalPos = lastCheckpointLine.find(" total=");
        size_t msgPos = lastCheckpointLine.find(" message=");

        if (stepPos != std::string::npos && totalPos != std::string::npos)
        {
            std::string stepStr = lastCheckpointLine.substr(stepPos + 5, totalPos - stepPos - 5);
            cp.step = std::stoi(stepStr);

            if (msgPos != std::string::npos)
            {
                std::string totalStr = lastCheckpointLine.substr(totalPos + 7, msgPos - totalPos - 7);
                cp.total = std::stoi(totalStr);
                cp.message = lastCheckpointLine.substr(msgPos + 9);
            }
            else
            {
                cp.total = std::stoi(lastCheckpointLine.substr(totalPos + 7));
            }
            cp.valid = true;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Logger] Failed to parse checkpoint: " << e.what() << std::endl;
        cp.valid = false;
    }

    return cp;
}

// ============================================================================
// 内部工具函数 / Internal utility functions
// ============================================================================

std::string Logger::getTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::levelToString(Level level) const
{
    switch (level)
    {
        case DEBUG: return "DEBUG";
        case INFO:  return "INFO ";
        case WARN:  return "WARN ";
        case ERROR: return "ERROR";
        default:    return "?????";
    }
}
