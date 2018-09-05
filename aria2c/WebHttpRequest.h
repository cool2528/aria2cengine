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
	std::string strUrl;	//��Ҫ�������ַ
	std::string strData;	//post ��Ҫ���͵�����
	HEADERLIST* headerList;	//��Ҫ�����Э��ͷ
	std::string strCookie;	//��Ҫ����cookie
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
	ȡ���ı��м�����
	@GetTextMid
	*/
	static std::string GetTextMid(const std::string strSource, const std::string strLeft, const std::string strRight);
	/*
	@��ȡcookies
	*/
	static std::string GetCookies(const std::string strHeader);
	/*
	@ �����ļ�
	*/
	static std::string DownloadFile(const std::string strUrl,const std::string strSavePath);
	/*
	@ ��ʽ���ַ���
	*/
	static std::string FormatText(const char* format, ...);
	/***************************************************/
	// ���� szSource ��Ҫ��URL������ַ���
	//���� isletter �Ƿ񲻱���������ĸ Ĭ��Ϊtrue ������ false �����
	//���� isUtf8 �Ƿ����ΪUTF-8��ʽ Ĭ��Ϊtrue UTF-8���� false �򲻱���Ϊutf-8
	/***************************************************/
	static std::string URL_Coding(const char* szSource, bool isletter=true, bool isUtf8=true);
	/*****************************************************/
	//����\uxxxx\uxxxx�����ַ�
	static std::string UnEscape(const char* strSource);
	// ����ת��
	static std::string Unicode_To_Ansi(const wchar_t* szbuff);
	static std::wstring Ansi_To_Unicode(const char* szbuff);
	static std::string Gbk_To_Utf8(const char* szBuff);
	static std::string Utf8_To_Gbk(const char* szBuff);
	/*****************************************************/
	//����ͼƬ�õ�ͼƬ���ֽ���
	static MEMORYBUF GetStreamBuf(const std::string strUrl);
	/******************************************************/
	static std::string Lua_Get_String(std::string strUrl, std::string& RetHeader, std::string strHeader = "", std::string strCookies = "");
	/**********************************************************/
	static std::string Lua_Post_String(std::string strUrl, std::string& RetHeader, std::string strSendData = "", std::string strHeader = "", std::string strCookies = "");
	/****************************************************/
private:
	/****************************************************/
	// post ���� ������ο� REQUESTINFO
	// �ɹ� ���� ������ ʧ�� ����ʧ�ܽ�� 
	/****************************************************/
	static std::string Curl_post_data(REQUESTINFO urlInfo, std::string& strRetHeader);
	/****************************************************/
	// get ���� ������ο� REQUESTINFO
	// �ɹ� ���� ������ ʧ�� ����ʧ�ܽ�� 
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
