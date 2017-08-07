#pragma once

struct ILog {
	void Log(const char *format, ...) {}
	void LogToConsole(const char *format, ...) {}
	void LogError(const char *format, ...) {}
};