#include <iostream>
#include <map>
#include <chrono>

std::map<std::string, std::chrono::high_resolution_clock::time_point> timestamps;

extern "C" {
	void __instrument_before_call(char* fnName);
	void __instrument_after_call(char* fnName);
}

void __instrument_before_call(char* fnName)
{
	timestamps[fnName] = std::chrono::high_resolution_clock::now();
}

void __instrument_after_call(char* fnName)
{
	size_t ms = std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::high_resolution_clock::now() - timestamps[fnName]).count();
	size_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>
		(std::chrono::high_resolution_clock::now() - timestamps[fnName]).count();


	std::cout << "[time.cpp] Function " << fnName << " took " << ms << " ms (" << ns <<  " ns)\n";
}
