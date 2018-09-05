#ifndef __WEBHTTPREQUEST__
#define __WEBHTTPREQUEST__
#include <curl/curl.h>
#include <vector>
#include <string>
#include <set>
#include <time.h>
#include <sys/timeb.h>
typedef std::vector<std::string> HEADERLIST;
typedef std::vector<char> MEMORYBUF;
typedef struct RequestInfo
{
	std::string strUrl;	//需要请求的网址
	std::string strData;	//post 需要发送的数据
	HEADERLIST* headerList;	//需要加入的协议头
	std::string strCookie;	//需要设置cookie
}REQUESTINFO, *PREQUESTINFO;
#define GB2312_ACP 936
class WebHttpRequest
{
public:
	WebHttpRequest();
	~WebHttpRequest();
public:
	static std::string GetLogid(bool isFlag=true);
	static std::string random();
	static long long getTimeStamp();
	/*
	取出文本中间内容
	@GetTextMid
	*/
	static std::string GetTextMid(const std::string strSource, const std::string strLeft, const std::string strRight);
	/*
	@提取cookies
	*/
	static std::string GetCookies(const std::string strHeader);
	/*
	@ 下载文件
	*/
	static std::string DownloadFile(const std::string strUrl,const std::string strSavePath);
	/*
	@ 格式化字符串
	*/
	static std::string FormatText(const char* format, ...);
	/***************************************************/
	// 参数 szSource 需要被URL编码的字符串
	//参数 isletter 是否不编码数字字母 默认为true 不编码 false 则编码
	//参数 isUtf8 是否编码为UTF-8格式 默认为true UTF-8编码 false 则不编码为utf-8
	/***************************************************/
	static std::string URL_Coding(const char* szSource, bool isletter=true, bool isUtf8=true);
	/*****************************************************/
	//解析\uxxxx\uxxxx编码字符
	static std::string UnEscape(const char* strSource);
	// 编码转换
	static std::string Unicode_To_Ansi(const wchar_t* szbuff);
	static std::wstring Ansi_To_Unicode(const char* szbuff);
	static std::string Gbk_To_Utf8(const char* szBuff);
	static std::string Utf8_To_Gbk(const char* szBuff);
	/*****************************************************/
	//访问图片得到图片的字节流
	static MEMORYBUF GetStreamBuf(const std::string strUrl);
	/******************************************************/
	static std::string Lua_Get_String(std::string strUrl, std::string& RetHeader, std::string strHeader = "", std::string strCookies = "");
	/**********************************************************/
	static std::string Lua_Post_String(std::string strUrl, std::string& RetHeader, std::string strSendData = "", std::string strHeader = "", std::string strCookies = "");
	/****************************************************/
private:
	/****************************************************/
	// post 请求 参数请参考 REQUESTINFO
	// 成功 返回 请求结果 失败 返回失败结果 
	/****************************************************/
	static std::string Curl_post_data(REQUESTINFO urlInfo, std::string& strRetHeader);
	/****************************************************/
	// get 请求 参数请参考 REQUESTINFO
	// 成功 返回 请求结果 失败 返回失败结果 
	/****************************************************/
	static std::string Curl_get_data(REQUESTINFO urlInfo, std::string& strRetHeader);
private:
	static size_t write_image_data(char *ptr, size_t size, size_t nmemb, MEMORYBUF* stream);
	static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);
	static size_t header_callback(char *buffer, size_t size, size_t nitems, std::string&userdata);
	static size_t write_callback(char *ptr, size_t size, size_t nmemb, std::string& userdata);
	static size_t read_callback(char *ptr, size_t size, size_t nmemb, std::string& userdata);
};
#endif
