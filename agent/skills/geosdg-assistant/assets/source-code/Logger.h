/**
 * @file Logger.h
 * @brief 日志工具类 / Logger utility class
 *
 * 提供文件+控制台双通道日志记录，支持分级日志、时间戳、执行进度追踪、
 * 断点续传（checkpoint）功能，便于长时间计算任务的中断恢复。
 *
 * Provides dual-channel (file + console) logging with level filtering,
 * timestamps, execution progress tracking, and checkpoint functionality
 * for resuming long-running computation tasks.
 *
 * 用法 / Usage:
 *   Logger::init("logs/geosdg.log");          // 初始化日志文件 / Initialize log file
 *   Logger::info("Starting calculation");       // 记录信息 / Log info
 *   Logger::warn("Threshold is zero");          // 记录警告 / Log warning
 *   Logger::error("File not found");            // 记录错误 / Log error
 *   Logger::writeCheckpoint(2, 6, "SDG done");  // 写入断点 / Write checkpoint
 *   int step = Logger::readLastCheckpoint(...); // 读取断点 / Read checkpoint
 */

#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ctime>
#include <sstream>

class Logger
{
public:
    /**
     * @brief 日志级别 / Log level
     */
    enum Level
    {
        DEBUG = 0,  ///< 调试信息 / Debug message
        INFO  = 1,  ///< 一般信息 / Informational message
        WARN  = 2,  ///< 警告 / Warning condition
        ERROR = 3   ///< 错误 / Error condition
    };

    /**
     * @brief 断点续传数据结构 / Checkpoint data structure
     *
     * 用于记录和恢复长时间计算任务的执行进度。
     * Used to record and resume execution progress of long-running tasks.
     */
    struct Checkpoint
    {
        int         step    = 0;    ///< 当前步骤编号 / Current step number
        int         total   = 0;    ///< 总步骤数 / Total steps
        std::string message;       ///< 步骤描述 / Step description
        bool        valid   = false; ///< 是否有效 / Whether valid
    };

    /**
     * @brief 获取单例实例 / Get singleton instance
     * @return Logger单例引用 / Reference to Logger singleton
     */
    static Logger& instance();

    /**
     * @brief 初始化日志系统 / Initialize logging system
     *
     * 设置日志文件路径，若文件已存在则追加写入。
     * 自动创建日志文件所在目录。
     * Sets the log file path; appends if the file already exists.
     * Automatically creates the log file's parent directory.
     *
     * @param logFilePath 日志文件路径 / Path to log file
     * @param minLevel    最小输出级别（低于此级别的日志不记录）/ Minimum output level
     */
    void init(const std::string& logFilePath, Level minLevel = DEBUG);

    /**
     * @brief 写入日志 / Write log entry
     * @param level   日志级别 / Log level
     * @param message 日志消息 / Log message
     */
    void log(Level level, const std::string& message);

    /** @brief 记录DEBUG级别日志 / Log DEBUG level message */
    void debug(const std::string& message);

    /** @brief 记录INFO级别日志 / Log INFO level message */
    void info(const std::string& message);

    /** @brief 记录WARN级别日志 / Log WARN level message */
    void warn(const std::string& message);

    /** @brief 记录ERROR级别日志 / Log ERROR level message */
    void error(const std::string& message);

    /**
     * @brief 记录函数执行结果 / Log function execution result
     *
     * 用于追踪中间计算结果，便于断点续传和结果回溯。
     * Tracks intermediate computation results for resumption and result tracing.
     *
     * @param funcName 函数名 / Function name
     * @param key      结果键名 / Result key name
     * @param value    结果值 / Result value
     */
    void result(const std::string& funcName, const std::string& key, double value);

    /**
     * @brief 记录步骤进度 / Log step progress
     *
     * @param step    步骤编号 / Step number
     * @param total   总步骤数 / Total steps
     * @param message 步骤描述 / Step description
     */
    void progress(int step, int total, const std::string& message);

    /**
     * @brief 写入断点续传标记 / Write checkpoint marker
     *
     * 将当前执行进度写入日志文件，格式为 [CHECKPOINT] step=X total=Y message=...
     * 用于支持长时间计算任务的中断恢复。
     *
     * Writes current execution progress to log file in format [CHECKPOINT] step=X total=Y message=...
     * Supports resumption of long-running computation tasks.
     *
     * @param step    当前步骤编号 / Current step number
     * @param total   总步骤数 / Total steps
     * @param message 步骤描述 / Step description
     */
    void writeCheckpoint(int step, int total, const std::string& message);

    /**
     * @brief 读取最近一次断点续传标记 / Read last checkpoint marker
     *
     * 从日志文件中解析最后一次 [CHECKPOINT] 行，返回步骤信息。
     * 若日志文件不存在或无断点，返回 invalid=false。
     *
     * Parses the last [CHECKPOINT] line from the log file.
     * Returns invalid=false if file doesn't exist or no checkpoint found.
     *
     * @return 断点数据结构 / Checkpoint data structure
     */
    Checkpoint readLastCheckpoint();

    /**
     * @brief 关闭日志文件 / Close log file
     */
    void shutdown();

    /**
     * @brief 获取当前日志文件路径 / Get current log file path
     * @return 日志文件路径 / Log file path
     */
    std::string getLogFilePath() const;

    // 禁止拷贝和移动 / Non-copyable, non-movable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    /**
     * @brief 获取当前时间戳字符串 / Get current timestamp string
     * @return 格式化时间戳 "YYYY-MM-DD HH:MM:SS"
     */
    std::string getTimestamp() const;

    /**
     * @brief 将日志级别转为字符串 / Convert log level to string
     * @param level 日志级别 / Log level
     * @return 级别字符串 / Level string
     */
    std::string levelToString(Level level) const;

    /**
     * @brief 确保日志文件所在目录存在 / Ensure log file's parent directory exists
     * @param filePath 文件路径 / File path
     * @return 是否成功 / Whether successful
     */
    bool ensureLogDirectory(const std::string& filePath);

    std::ofstream logFile_;       ///< 日志文件输出流 / Log file output stream
    std::string   logFilePath_;   ///< 日志文件路径 / Log file path
    Level         minLevel_ = DEBUG; ///< 最小输出级别 / Minimum output level
    bool          initialized_ = false; ///< 是否已初始化 / Whether initialized
};

// ── 便捷宏：简化日志调用 / Convenience macros for logging ──
// 使用方式 / Usage: LOG_INFO("message"), LOG_WARN("message"), etc.
#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
#define LOG_RESULT(fn, k, v) Logger::instance().result(fn, k, v)
#define LOG_PROGRESS(s, t, m) Logger::instance().progress(s, t, m)
#define LOG_CHECKPOINT(s, t, m) Logger::instance().writeCheckpoint(s, t, m)
