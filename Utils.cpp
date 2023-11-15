#include "Utils.h"


//YUVStruct::~YUVStruct()
//{
//	if (data)
//	{
//		free(data);
//	}
//}
//YUVStruct::YUVStruct(char* data, int32_t size, int32_t width, int32_t height)
//	:size(size), width(width), height(height)
//{
//	this->data = (char*)malloc(size);
//	memcpy(this->data, data, size);
//}
//
//YUVStruct::YUVStruct(int32_t size, int32_t width, int32_t height)
//	:size(size), width(width), height(height)
//{
//	this->data = (char*)malloc(size);
//}
//
//YUV420p::YUV420p(int32_t size, int32_t width, int height) : YUVStruct(size, width, height)
//{
//	int frame = width * height;
//	Y = data;
//	U = data + frame;
//	V = data + frame * 5 / 4;
//}
//YUV420p::YUV420p(char* data, int32_t size, int32_t width, int height) :YUVStruct(data, size, width, height)
//{
//	int frame = width * height;
//	Y = data;
//	U = data + frame;
//	V = data + frame * 5 / 4;
//}
//YUV420p::~YUV420p()
//{
//
//}
//
//NaluStruct::NaluStruct(int size)
//{
//	this->size = size;
//	type = 0;
//	data = (unsigned char*)malloc(size * sizeof(char));
//}
//
//NaluStruct::NaluStruct(const unsigned char* buf, int bufLen)
//{
//	this->size = bufLen;
//	type = buf[4] & 0x1f;
//	data = (unsigned char*)malloc(bufLen * sizeof(char));
//	memcpy(data, buf, bufLen);
//}
//
//NaluStruct::~NaluStruct()
//{
//	if (data)
//	{
//		free(data);
//		data = NULL;
//	}
//}
