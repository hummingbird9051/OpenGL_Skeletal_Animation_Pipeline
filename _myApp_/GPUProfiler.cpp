#include "GPUProfiler.h"
#include<iostream>

static const char* names[GPULabelCount] = { "GPURendering" };

GPUProfiler::GPUProfiler() {
	glGenQueries(GPULabelCount * GPUqueriesPerLabel, &queries[0][0]);
	lastUpdate = std::chrono::steady_clock::now();
}

GPUProfiler::~GPUProfiler() {
	glDeleteQueries(GPULabelCount * GPUqueriesPerLabel, &queries[0][0]);
}

void GPUProfiler::beginQuery(GPULabels queryId) {
	int frame = frameCount % GPUqueriesPerLabel;
	glBeginQuery(GL_TIME_ELAPSED, queries[queryId][frame]);
}

void GPUProfiler::endQuery(GPULabels queryId) {
	glEndQuery(GL_TIME_ELAPSED);
	if (frameCount < GPUqueriesPerLabel - 1) return;
	int frame = (frameCount + 1) % GPUqueriesPerLabel;
	GLint isResultAvailable = GL_FALSE;
	glGetQueryObjectiv(queries[queryId][frame], GL_QUERY_RESULT_AVAILABLE, &isResultAvailable);
	if (isResultAvailable == GL_TRUE) {
		GLint64 result;
		glGetQueryObjecti64v(queries[queryId][frame], GL_QUERY_RESULT, &result);
		querySum[queryId] += result / 1000000.0;
		framePerTick[queryId] += 1;
	}
}

void GPUProfiler::nextFrame() {
	frameCount++;
}

void GPUProfiler::printQuery() {
	auto now = std::chrono::steady_clock::now();
	if (now - lastUpdate < std::chrono::seconds(1)) return;
	for (int i = 0; i < GPULabelCount; i++) {
		if (framePerTick[i] == 0) continue;
		double average = querySum[i] / framePerTick[i];
		std::cout << names[i] << "'s time average for 1sec is " << average << "ms" << std::endl;
		averages[i] = average;
		querySum[i] = 0.0f;
		framePerTick[i] = 0;
	}
	lastUpdate = now;
}

double GPUProfiler::getAverage(int label) {
	return averages[label];
}