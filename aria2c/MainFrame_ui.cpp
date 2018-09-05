#include "MainFrame_ui.h"
#include "resource.h"
#include "WebHttpRequest.h"
#include <fstream>
#include <Shlobj.h>
#include <DbgHelp.h>
#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"SHELL32.LIB")
#define  GB2312_ACP 936
HINSTANCE g_hInstance = nullptr;
std::string MainFrame_ui::m_vcode;
std::string MainFrame_ui::m_password;
std::string MainFrame_ui::m_vcCodeUrl;
typedef websocketpp::client<websocketpp::config::asio_client> client;
typedef std::vector<std::string> gidArray;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
typedef std::shared_ptr<std::thread> SharedThread;
typedef std::vector<SharedThread> ThreadArr;
// pull out the type of messages sent by our config
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::connection_ptr connection_ptr;
std::mutex mut;
class perftest {
public:
	typedef perftest type;
	typedef std::chrono::duration<int, std::micro> dur_type;

	perftest(MainFrame_ui* FramePtr) :m_FrameDlgPtr(FramePtr){
		m_endpoint.set_access_channels(websocketpp::log::alevel::all);
		m_endpoint.set_error_channels(websocketpp::log::elevel::all);

		// Initialize ASIO
		m_endpoint.init_asio();

		// Register our handlers
		m_endpoint.set_socket_init_handler(bind(&type::on_socket_init, this, ::_1));
		//m_endpoint.set_tls_init_handler(bind(&type::on_tls_init,this,::_1));
		m_endpoint.set_message_handler(bind(&type::on_message, this, ::_1, ::_2));
		m_endpoint.set_open_handler(bind(&type::on_open, this, ::_1));
		m_endpoint.set_close_handler(bind(&type::on_close, this, ::_1));
		m_endpoint.set_fail_handler(bind(&type::on_fail, this, ::_1));
	}
	~perftest()
	{
		m_GidVector.clear(); //让线程自动退出
		for (auto& v : m_ThreadPtr)
		{
			if (v->joinable())
			{
				v->join();
			}
			v.reset();
		}
	}
	void start(std::string uri) {
		websocketpp::lib::error_code ec;
		client::connection_ptr con = m_endpoint.get_connection(uri, ec);
		if (ec) {
			m_endpoint.get_alog().write(websocketpp::log::alevel::app, ec.message());
			return;
		}
		m_endpoint.connect(con);
		m_start = std::chrono::high_resolution_clock::now();
		m_endpoint.run();
	}

	void on_socket_init(websocketpp::connection_hdl) {
		m_socket_init = std::chrono::high_resolution_clock::now();
	}

	context_ptr on_tls_init(websocketpp::connection_hdl) {
		m_tls_init = std::chrono::high_resolution_clock::now();
		context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);

		try {
			ctx->set_options(boost::asio::ssl::context::default_workarounds |
				boost::asio::ssl::context::no_sslv2 |
				boost::asio::ssl::context::no_sslv3 |
				boost::asio::ssl::context::single_dh_use);
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
		}
		return ctx;
	}

	void on_fail(websocketpp::connection_hdl hdl) {
		client::connection_ptr con = m_endpoint.get_con_from_hdl(hdl);

		std::cout << "Fail handler" << std::endl;
		std::cout << con->get_state() << std::endl;
		std::cout << con->get_local_close_code() << std::endl;
		std::cout << con->get_local_close_reason() << std::endl;
		std::cout << con->get_remote_close_code() << std::endl;
		std::cout << con->get_remote_close_reason() << std::endl;
		std::cout << con->get_ec() << " - " << con->get_ec().message() << std::endl;
	}

	void on_open(websocketpp::connection_hdl hdl) {
		m_open = std::chrono::high_resolution_clock::now();
		m_hdl = hdl;
	}
	void shutdown()
	{
		send(Gbk_To_Utf8("{\"jsonrpc\":\"2.0\",\"method\":\"aria2.shutdown\",\"params\":[\"token:123\"],\"id\":1}"));
	}
	void unpauseAll()
	{
		send(Gbk_To_Utf8("{\"jsonrpc\":\"2.0\",\"method\":\"aria2.forcePauseAll\",\"params\":[\"token:123\"],\"id\":1}"));
	}
	void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
		m_message = std::chrono::high_resolution_clock::now();
		websocketpp::frame::opcode::value opcodes = msg->get_opcode();
		if (opcodes == websocketpp::frame::opcode::text)
		{
			std::string buf = msg->get_payload();
			std::string strresult;
			std::cout << "and message to: " << buf << std::endl;
			rapidjson::Document dc;
			dc.Parse(buf.c_str());
			if (!dc.IsObject())
			{
				return;
			}
			if (dc.HasMember("result") && dc["result"].IsString())
			{
				strresult = dc["result"].GetString();
				m_GidVector.push_back(strresult);
			}
			if (dc.HasMember("result") && dc["result"].IsObject())
			{
				PrintState(buf);
			}
			if (dc.HasMember("method") && dc["method"].IsString())
			{
				std::string strResult = dc["method"].GetString();
				if (strResult == "aria2.onDownloadStart")	//开始下载
				{
					m_isDownloadOk = true;
					SharedThread ptrbuf;
					ptrbuf.reset(new std::thread(&perftest::ThreadProc, this, ""));
					m_ThreadPtr.push_back(ptrbuf);
					std::string strGidComplete;
					rapidjson::Value paramsArr;
					if (dc.HasMember("params") && dc["params"].IsArray())
					{
						paramsArr = dc["params"];
						for (auto& v : paramsArr.GetArray())
						{
							if (!v.IsObject())
								continue;
							if (v.HasMember("gid") && v["gid"].IsString())
							{
								strGidComplete = v["gid"].GetString();
							}
						}
					}
					DOWNLOADINFO argInfo;
					ZeroMemory(&argInfo, sizeof(DOWNLOADINFO));
					argInfo.szDownloadProgress = _T("0%");
					argInfo.szDownloadSpeed = _T("0 KB");
					argInfo.szDownloadStatus = _T("active");
					argInfo.szGid = Ansi_To_Unicode(strGidComplete.c_str()).c_str();
					m_FrameDlgPtr->InsertListText(&argInfo);
				}
				else if (strResult == "aria2.onDownloadPause")	//暂停下载
				{

				}
				else if (strResult == "aria2.onDownloadStop")	//用户自己停止下载
				{

				}
				else if (strResult == "aria2.onDownloadComplete")	//下载完成提示
				{
					std::string strGidComplete;
					rapidjson::Value paramsArr;
					if (dc.HasMember("params") && dc["params"].IsArray())
					{
						paramsArr = dc["params"];
						for (auto& v : paramsArr.GetArray())
						{
							if (!v.IsObject())
								continue;
							if (v.HasMember("gid") && v["gid"].IsString())
							{
								strGidComplete = v["gid"].GetString();
							}
						}
					}
					gidArray::iterator it;
					mut.lock();
					for (it = m_GidVector.begin(); it != m_GidVector.end(); it++)
					{
						if (strGidComplete == *it)
						{

							m_GidVector.erase(it);
							break;
						}
					}
					mut.unlock();
					DOWNLOADINFO argInfo;
					ZeroMemory(&argInfo, sizeof(DOWNLOADINFO));
					argInfo.szDownloadProgress = _T("100%");
					argInfo.szDownloadSpeed = _T("0 KB");
					argInfo.szDownloadStatus = _T("下载完成");
					argInfo.szGid = Ansi_To_Unicode(strGidComplete.c_str()).c_str();
					argInfo.szFileName = m_FrameDlgPtr->GetlistItemText(argInfo.szGid.c_str()).GetData();
					m_FrameDlgPtr->UpdateListItem(&argInfo);
					//std::cout << "文件下载完成！" << std::endl;
					strGidComplete += "---------------------下载完成\n";
					OutputDebugStringA(strGidComplete.c_str());
					m_FrameDlgPtr->InsetText(Ansi_To_Unicode(strGidComplete.c_str()).c_str());
					//m_endpoint.close(hdl, websocketpp::close::status::going_away, "");
				}
				else if (strResult == "aria2.onDownloadError")	//出现错误导致下载停止
				{

				}
				else if (strResult == "aria2.onBtDownloadComplete")	//当torrent下载完成但播种仍在继续时，将发送此通知
				{
				}
			}
		}
		m_hdl = hdl;
		//m_endpoint.close(hdl, websocketpp::close::status::going_away, "");
	}
	void on_close(websocketpp::connection_hdl hdl) {
		m_close = std::chrono::high_resolution_clock::now();

		//std::cout << "Socket Init: " << std::chrono::duration_cast<dur_type>(m_socket_init - m_start).count() << std::endl;
		//std::cout << "TLS Init: " << std::chrono::duration_cast<dur_type>(m_tls_init - m_start).count() << std::endl;
		//std::cout << "Open: " << std::chrono::duration_cast<dur_type>(m_open - m_start).count() << std::endl;
		//std::cout << "Message: " << std::chrono::duration_cast<dur_type>(m_message - m_start).count() << std::endl;
		//std::cout << "Close: " << std::chrono::duration_cast<dur_type>(m_close - m_start).count() << std::endl;
		m_hdl = hdl;
	}
	bool send(std::string buf)
	{
		bool isok = false;
		if (m_hdl.lock().get())
		{
			m_endpoint.send(m_hdl, buf, websocketpp::frame::opcode::text);
			isok = true;
		}
		return isok;
	}
	void ws_stop()
	{
		if (m_hdl.lock().get())
		{
			m_endpoint.close(m_hdl, websocketpp::close::status::going_away, "");
		}
	}
	void ThreadProc(std::string arg)
	{
		if (m_GidVector.empty())
		{
			return;
		}
		while (m_GidVector.size())
		{
			mut.lock();
			for (auto& v : m_GidVector)
			{
				std::string strData = "{\"jsonrpc\":\"2.0\", \"id\":\"qwer\", \"method\":\"aria2.tellStatus\",\"params\":[\"";
				strData += v;
				strData += "\"]}";
				send(Gbk_To_Utf8(strData.c_str()));

			}
			mut.unlock();
			Sleep(1000);
		}
	}
	void PrintState(std::string buf)
	{
		DOWNLOADINFO ArgDownloadInfo;
		ZeroMemory(&ArgDownloadInfo, sizeof(DOWNLOADINFO));
		rapidjson::Document dc;
		ULONGLONG totalLength, completedLength, downloadSpeed;
		std::string status, strOutText,strDownProgress,strFileGid,strFileName;
		CDuiString strDownloadSpeed;
		dc.Parse(buf.c_str());
		if (!dc.IsObject())
		{
			return;
		}
		rapidjson::Value resultobj;
		if (dc.HasMember("result") && dc["result"].IsObject())
		{
			resultobj = dc["result"];
			if (resultobj.HasMember("totalLength") && resultobj["totalLength"].IsString())
			{
				//totalLength = atoi(resultobj["totalLength"].GetString());	//文件总大小
				sscanf_s(resultobj["totalLength"].GetString(), "%I64d", &totalLength);
			}
			if (resultobj.HasMember("completedLength") && resultobj["completedLength"].IsString())
			{
				//completedLength = atoi(resultobj["completedLength"].GetString());	//当前下载大小
				sscanf_s(resultobj["completedLength"].GetString(), "%I64d", &completedLength);
			}
			if (resultobj.HasMember("downloadSpeed") && resultobj["downloadSpeed"].IsString())
			{
				//downloadSpeed = atoi(resultobj["downloadSpeed"].GetString());	//当前下载速度
				sscanf_s(resultobj["downloadSpeed"].GetString(), "%I64d", &downloadSpeed);
			}
			if (resultobj.HasMember("status") && resultobj["status"].IsString())
			{
				status = resultobj["status"].GetString();	//当前状态
			}
			if (resultobj.HasMember("gid") && resultobj["gid"].IsString())
			{
				strFileGid = resultobj["gid"].GetString();
			}
			if (resultobj.HasMember("files") && resultobj["files"].IsArray())
			{
				for (auto& v: resultobj["files"].GetArray())
				{
					if (!v.IsObject())
					{
						continue;
					}
					if (v.HasMember("path") && v["path"].IsString())
					{
						strFileName = v["path"].GetString();
						//./downloads/tim_pc.exe
						if (!strFileName.empty())
						{
							int npos = strFileName.rfind("/");
							if (npos != std::string::npos)
							{
								strFileName = strFileName.substr(npos+1, strFileName.length() - npos);
							}
						}
						break;
					}
				}
			}
		}
		int nDownloadState = (((double)completedLength / (double)totalLength) * 100);
		if (nDownloadState<0)
			nDownloadState = 0;
		strDownProgress = FormatText("%d%%",nDownloadState);

		strDownloadSpeed = m_FrameDlgPtr->GetFileSizeType((double)downloadSpeed);
		std::wstring wstrbuf = Ansi_To_Unicode(strDownProgress.c_str());
		ArgDownloadInfo.szDownloadProgress = wstrbuf.empty() ? _T("0%") : wstrbuf.c_str();
		ArgDownloadInfo.szDownloadSpeed = strDownloadSpeed;
		wstrbuf = Ansi_To_Unicode(status.c_str());
		ArgDownloadInfo.szDownloadStatus = wstrbuf.empty() ? _T("cache") : wstrbuf.c_str();
		wstrbuf = Ansi_To_Unicode(strFileName.c_str());
		ArgDownloadInfo.szFileName = wstrbuf.empty() ? _T("loadfile") : wstrbuf.c_str();
		wstrbuf = Ansi_To_Unicode(strFileGid.c_str());
		ArgDownloadInfo.szGid = wstrbuf.empty() ? _T("nogid") : wstrbuf.c_str();
		strOutText = FormatText("当前下载进度:%s 每秒下载速度：%s", strDownProgress.c_str(), Unicode_To_Ansi(strDownloadSpeed.GetData()).c_str());
		strOutText += "\n";
		OutputDebugStringA(strOutText.c_str());
		m_FrameDlgPtr->UpdateListItem(&ArgDownloadInfo);
		//m_FrameDlgPtr->InsetText(Ansi_To_Unicode(strOutText.c_str()).c_str());
		//std::cout << strOutText << std::endl;
	}
private:
	bool m_isDownloadOk;
	MainFrame_ui* m_FrameDlgPtr;
	client m_endpoint;
	gidArray m_GidVector;
	ThreadArr m_ThreadPtr;
	websocketpp::connection_hdl m_hdl;
	std::chrono::high_resolution_clock::time_point m_start;
	std::chrono::high_resolution_clock::time_point m_socket_init;
	std::chrono::high_resolution_clock::time_point m_tls_init;
	std::chrono::high_resolution_clock::time_point m_open;
	std::chrono::high_resolution_clock::time_point m_message;
	std::chrono::high_resolution_clock::time_point m_close;
};
bool checkVersion()
{
	bool isShow = true;
	int nOk = -1;
	std::string strResult, strRetHeader, strVersion;
	std::string strUrl = "https://www.jianshu.com/p/c97bbfc9392e";
	strResult = WebHttpRequest::Lua_Get_String(strUrl, strRetHeader);
	strResult = WebHttpRequest::Utf8_To_Gbk(strResult.c_str());
	strVersion = WebHttpRequest::GetTextMid(strResult, "<p>友元类运算符{", "}");
	if (strVersion != "180905")
	{
		strVersion = WebHttpRequest::GetTextMid(strResult, "如果无法打开｛", "｝这里下载就行了");
		nOk = MessageBoxA(NULL, "发现有新版本是否去下载更新！", "update", MB_YESNOCANCEL);
		if (nOk == IDYES)
		{

			nOk = (int)ShellExecuteA(NULL, "open", strVersion.c_str(), NULL, NULL, SW_SHOWMAXIMIZED);
			isShow = false;
			return isShow;
		}

	}
	return isShow;
}
MainFrame_ui::MainFrame_ui()
{
	m_perftest = nullptr;
}


MainFrame_ui::~MainFrame_ui()
{
	if (m_RunThreadPtr)
	{
		m_perftest->unpauseAll();
		m_perftest->shutdown();
		m_perftest->ws_stop();
		if (m_RunThreadPtr->joinable())
		{
			m_RunThreadPtr->join();
		}
		m_RunThreadPtr.reset();
	}
	if (m_perftest)
	{
		delete m_perftest;
		m_perftest = NULL;
	}
}
bool MainFrame_ui::Remove(CDuiString szGid)
{
	bool isRest = false;
	if (szGid.IsEmpty())return isRest;
	std::string strGid = Unicode_To_Ansi(szGid.GetData());
	strGid = FormatText("{\"jsonrpc\":\"2.0\", \"id\":\"qwer\",\"method\":\"aria2.remove\",\"params\":[\"%s\"]}", strGid.c_str());
	if (m_perftest)
		isRest = m_perftest->send(Gbk_To_Utf8(strGid.c_str()));
	return isRest;
}

bool MainFrame_ui::pause(CDuiString szGid)
{
	bool isRest = false;
	if (szGid.IsEmpty())return isRest;
	std::string strGid = Unicode_To_Ansi(szGid.GetData());
	strGid = FormatText("{\"jsonrpc\":\"2.0\", \"id\":\"qwer\",\"method\":\"aria2.pause\",\"params\":[\"%s\"]}", strGid.c_str());
	if (m_perftest)
		isRest = m_perftest->send(Gbk_To_Utf8(strGid.c_str()));
	return isRest;
}

bool MainFrame_ui::unpause(CDuiString szGid)
{
	bool isRest = false;
	if (szGid.IsEmpty())return isRest;
	std::string strGid = Unicode_To_Ansi(szGid.GetData());
	strGid = FormatText("{\"jsonrpc\":\"2.0\", \"id\":\"qwer\",\"method\":\"aria2.unpause\",\"params\":[\"%s\"]}", strGid.c_str());
	if (m_perftest)
		isRest = m_perftest->send(Gbk_To_Utf8(strGid.c_str()));
	return isRest;
}

LPCTSTR MainFrame_ui::GetWindowClassName(void) const
{
	return _T("MainWindow");
}

std::string FormatText(const char* format, ...)
{
	std::string strRetbuf;
	char szBuffer[1024];
	ZeroMemory(szBuffer, 1024);
	va_list arglist;
	va_start(arglist, format);
	vsprintf_s(szBuffer, format, arglist);
	va_end(arglist);
	strRetbuf = szBuffer;
	return strRetbuf;
}
std::string Utf8_To_Gbk(const char* szBuff)
{
	std::string resault;
	int widLen = 0;
	int MultiLen = 0;
	WCHAR* widPtr = nullptr;
	CHAR* MulitPtr = nullptr;
	widLen = ::MultiByteToWideChar(CP_UTF8, NULL, szBuff, -1, NULL, NULL);//获取转换后需要的空间
	widPtr = new WCHAR[widLen + 1];
	if (!widPtr)
		return resault;
	ZeroMemory(widPtr, (widLen + 1) * sizeof(WCHAR));
	if (!::MultiByteToWideChar(CP_UTF8, NULL, szBuff, -1, widPtr, widLen))
	{
		delete[] widPtr;
		widPtr = nullptr;
		return resault;
	}
	MultiLen = ::WideCharToMultiByte(CP_ACP, NULL, widPtr, -1, NULL, NULL, NULL, NULL);
	if (MultiLen)
	{
		MulitPtr = new CHAR[MultiLen + 1];
		if (!MulitPtr)
		{
			delete[] widPtr;
			widPtr = nullptr;
			return resault;
		}
		ZeroMemory(MulitPtr, (MultiLen + 1) * sizeof(CHAR));
		::WideCharToMultiByte(CP_ACP, NULL, widPtr, -1, MulitPtr, MultiLen, NULL, NULL);
		resault = MulitPtr;
		delete[] MulitPtr;
		MulitPtr = nullptr;
	}
	delete[] widPtr;
	widPtr = nullptr;
	return resault;
}
std::string Gbk_To_Utf8(const char* szBuff)
{
	std::string resault;
	int widLen = 0;
	int MultiLen = 0;
	WCHAR* widPtr = nullptr;
	CHAR* MulitPtr = nullptr;
	widLen = ::MultiByteToWideChar(CP_ACP, NULL, szBuff, -1, NULL, NULL);//获取转换后需要的空间
	widPtr = new WCHAR[widLen + 1];
	if (!widPtr)
		return resault;
	ZeroMemory(widPtr, (widLen + 1) * sizeof(WCHAR));
	if (!::MultiByteToWideChar(CP_ACP, NULL, szBuff, -1, widPtr, widLen))
	{
		delete[] widPtr;
		widPtr = nullptr;
		return resault;
	}
	MultiLen = ::WideCharToMultiByte(CP_UTF8, NULL, widPtr, -1, NULL, NULL, NULL, NULL);
	if (MultiLen)
	{
		MulitPtr = new CHAR[MultiLen + 1];
		if (!MulitPtr)
		{
			delete[] widPtr;
			widPtr = nullptr;
			return resault;
		}
		ZeroMemory(MulitPtr, (MultiLen + 1) * sizeof(CHAR));
		::WideCharToMultiByte(CP_UTF8, NULL, widPtr, -1, MulitPtr, MultiLen, NULL, NULL);
		resault = MulitPtr;
		delete[] MulitPtr;
		MulitPtr = nullptr;
	}
	delete[] widPtr;
	widPtr = nullptr;
	return resault;
}

std::string Unicode_To_Ansi(const wchar_t* szbuff)
{
	std::string result;
	CHAR* MultPtr = nullptr;
	int MultLen = -1;
	MultLen = ::WideCharToMultiByte(GB2312_ACP, WC_COMPOSITECHECK, szbuff, -1, NULL, NULL, NULL, NULL);
	MultPtr = new CHAR[MultLen + 1];
	if (MultPtr)
	{
		ZeroMemory(MultPtr, MultLen + 1);
		::WideCharToMultiByte(GB2312_ACP, WC_COMPOSITECHECK, szbuff, -1, MultPtr, MultLen, NULL, NULL);
		result = MultPtr;
		delete[] MultPtr;
		MultPtr = nullptr;
	}
	return result;
}

std::wstring Ansi_To_Unicode(const char* szbuff)
{
	std::wstring result;
	WCHAR* widePtr = nullptr;
	int wideLen = -1;
	wideLen = ::MultiByteToWideChar(CP_ACP, NULL, szbuff, -1, NULL, NULL);
	widePtr = new WCHAR[wideLen + 1];
	if (widePtr)
	{
		ZeroMemory(widePtr, (wideLen + 1) * sizeof(WCHAR));
		::MultiByteToWideChar(CP_ACP, NULL, szbuff, -1, widePtr, wideLen);
		result = widePtr;
		delete[] widePtr;
		widePtr = nullptr;
	}
	return result;
}
LRESULT MainFrame_ui::ResponseDefaultKeyEvent(WPARAM wParam)
{
	switch (wParam)
	{
	case VK_RETURN:
		return 0;
	case VK_ESCAPE:
		return TRUE;
	default:
		break;
	}
	return __super::ResponseDefaultKeyEvent(wParam);
}

DuiLib::CDuiString MainFrame_ui::GetSkinFolder()
{
	return _T("");
}

DuiLib::CDuiString MainFrame_ui::GetSkinFile()
{
	return _T("MainFrame.xml");
}

void MainFrame_ui::OnFinalMessage(HWND hWnd)
{
	__super::OnFinalMessage(hWnd);
	delete this;
}

LRESULT MainFrame_ui::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return __super::HandleMessage(uMsg, wParam, lParam);
}

LRESULT MainFrame_ui::HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	return __super::HandleCustomMessage(uMsg, wParam, lParam, bHandled);
}

LRESULT MainFrame_ui::OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == SC_CLOSE) {
		::PostQuitMessage(0L);
		bHandled = TRUE;
		return 0;
	}
	BOOL bZoomed = ::IsZoomed(*this);
	LRESULT lRes = CWindowWnd::HandleMessage(uMsg, wParam, lParam);
	if (::IsZoomed(*this) != bZoomed) {
		if (!bZoomed) {
			CControlUI* pControl = static_cast<CControlUI*>(m_PaintManager.FindControl(_T("maxbtn")));
			if (pControl) pControl->SetVisible(false);
			pControl = static_cast<CControlUI*>(m_PaintManager.FindControl(_T("restorebtn")));
			if (pControl) pControl->SetVisible(true);
		}
		else {
			CControlUI* pControl = static_cast<CControlUI*>(m_PaintManager.FindControl(_T("maxbtn")));
			if (pControl) pControl->SetVisible(true);
			pControl = static_cast<CControlUI*>(m_PaintManager.FindControl(_T("restorebtn")));
			if (pControl) pControl->SetVisible(false);
		}
	}
	return lRes;
}

DuiLib::UILIB_RESOURCETYPE MainFrame_ui::GetResourceType() const
{
	return UILIB_ZIPRESOURCE;
}

LPCTSTR MainFrame_ui::GetResourceID() const
{
	return MAKEINTRESOURCE(IDR_ZIPRES1);
}
std::string MainFrame_ui::SelectResourcePath()
{
	BROWSEINFO broweInfo;
	LPITEMIDLIST pil = NULL;
	INITCOMMONCONTROLSEX InitCtrls;
	std::wstring strPath;
	TCHAR szPath[MAX_PATH];
	ZeroMemory(szPath, MAX_PATH);
	ZeroMemory(&InitCtrls, sizeof(INITCOMMONCONTROLSEX));
	ZeroMemory(&broweInfo, sizeof(BROWSEINFO));
	broweInfo.hwndOwner = GetHWND();
	broweInfo.lpszTitle = _T("选择需要保存到到文件路径");
	broweInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_STATUSTEXT;
	InitCommonControlsEx(&InitCtrls);
	pil = SHBrowseForFolder(&broweInfo);
	if (pil)
		SHGetPathFromIDList(pil, szPath);
	CoTaskMemFree(pil);
	strPath = szPath;
	if (strPath.empty())
	{
		strPath = m_downPath->GetText()+_T("\\");
	}
	m_downPath->SetText(strPath.c_str());
	return WebHttpRequest::Unicode_To_Ansi(strPath.c_str());
}
void MainFrame_ui::InitWindow()
{
	if (!checkVersion())
	{
		PostQuitMessage(0);
		return;
	}
	m_captionEx = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("captionEx")));
	ASSERT(m_captionEx);
	m_input = static_cast<CEditUI*>(m_PaintManager.FindControl(_T("input")));
	ASSERT(m_input);
	m_downPath = static_cast<CEditUI*>(m_PaintManager.FindControl(_T("downPath")));
	ASSERT(m_downPath);
	m_savePath = static_cast<CButtonUI*>(m_PaintManager.FindControl(_T("savePath")));
	ASSERT(m_savePath);
	m_jumpe = static_cast<CButtonUI*>(m_PaintManager.FindControl(_T("jumpUrl")));
	ASSERT(m_jumpe);
	m_logRich = static_cast<CRichEditUI*>(m_PaintManager.FindControl(_T("logRich")));
	ASSERT(m_logRich);
	m_main_Tablayout = static_cast<CTabLayoutUI*>(m_PaintManager.FindControl(_T("main_Tablayout")));
	ASSERT(m_main_Tablayout);
	m_listViewEx = static_cast<CListUI*>(m_PaintManager.FindControl(_T("listViewEx")));
	ASSERT(m_listViewEx);
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	DWORD dwSize = 0;
	PVOID pBuffer = nullptr;
	CHAR szPathFile[MAX_PATH];
	ZeroMemory(szPathFile, MAX_PATH);
	m_logRich->SetText(_T("本软件主要核心功能采用ari2c高速下载引擎\r\nhttps://aria2.github.io/\r\n提供加速下载网络资源文件\r\n目前支持百度云盘免登陆下载\r\n支持带私密分享链接与公开分享链接解析\r\n本软件开发只为学习交流使用！请勿用于非法用途！\r\n软件完全免费！免费！免费！\r\n不支持带有空格的路径保存文件\r\n修复已知BUG！"));
	dwSize = ReadFileBuffer("aria2.conf", &pBuffer);
	if (dwSize == INVALID_FILE_ATTRIBUTES && !pBuffer)
	{
		MessageBox(GetHWND(), _T("aria2.conf文件不存在"), NULL, MB_ERR_INVALID_CHARS);
		return;
	}
	std::string szTmpPath = WebHttpRequest::GetTextMid((char*)pBuffer, "dir=", "\r\n");
	delete[] pBuffer;
	pBuffer = nullptr;
	if (szTmpPath.empty())
	{
		rapidjson::Document dc;
		PVOID szBuffer = nullptr;
		dwSize = ReadFileBuffer("config.txt", &szBuffer);
		if (dwSize == INVALID_FILE_ATTRIBUTES && !szBuffer)
		{
			CHAR szPath[MAX_PATH];
			ZeroMemory(szPath, MAX_PATH);
			::GetCurrentDirectoryA(MAX_PATH, szPath);
			szTmpPath = szPath;
			szTmpPath += "\\download\\";
			std::string strPathconfig;
			rapidjson::Document dc;
			dc.SetObject();
			rapidjson::Document::AllocatorType& allocator = dc.GetAllocator();
			rapidjson::Value str(rapidjson::kStringType);
			str.SetString(szTmpPath.c_str(), szTmpPath.length(), allocator);
			dc.AddMember("downpath", str, allocator);
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			dc.Accept(writer);
			strPathconfig = buffer.GetString();
			WriteFileBuffer("config.txt", (char*)strPathconfig.c_str(), strPathconfig.length());
		}
		dc.Parse((char*)szBuffer);
		if (!dc.IsObject())
		{
			CHAR szPath[MAX_PATH];
			ZeroMemory(szPath, MAX_PATH);
			::GetCurrentDirectoryA(MAX_PATH, szPath);
			szTmpPath = szPath;
			szTmpPath += "\\download\\";
			std::string strPathconfig;
			rapidjson::Document dc;
			dc.SetObject();
			rapidjson::Document::AllocatorType& allocator = dc.GetAllocator();
			rapidjson::Value str(rapidjson::kStringType);
			str.SetString(szTmpPath.c_str(), szTmpPath.length(), allocator);
			dc.AddMember("downpath", str, allocator);
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			dc.Accept(writer);
			strPathconfig = buffer.GetString();
			WriteFileBuffer("config.txt", (char*)strPathconfig.c_str(), strPathconfig.length());
		}
		else
		{
			if (dc.HasMember("downpath") && dc["downpath"].IsString())
				szTmpPath = dc["downpath"].GetString();
			if (szTmpPath.empty())
			{
				CHAR szPath[MAX_PATH];
				ZeroMemory(szPath, MAX_PATH);
				::GetCurrentDirectoryA(MAX_PATH, szPath);
				szTmpPath = szPath;
				szTmpPath += "\\download\\";
				std::string strPathconfig;
				rapidjson::Document dc;
				dc.SetObject();
				rapidjson::Document::AllocatorType& allocator = dc.GetAllocator();
				rapidjson::Value str(rapidjson::kStringType);
				str.SetString(szTmpPath.c_str(), szTmpPath.length(), allocator);
				dc.AddMember("downpath", str, allocator);
				rapidjson::StringBuffer buffer;
				rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
				dc.Accept(writer);
				strPathconfig = buffer.GetString();
				WriteFileBuffer("config.txt", (char*)strPathconfig.c_str(), strPathconfig.length());
			}
		}
		delete[] szBuffer;
		szBuffer = nullptr;
	}
	if (MakeSureDirectoryPathExists(szTmpPath.c_str()))
	{
		//GetShortPathNameA(szTmpPath.c_str(), szPathFile, MAX_PATH);
		m_DownPath = Ansi_To_Unicode(szTmpPath.c_str());
	}
	else
	{
		m_DownPath = Ansi_To_Unicode(szTmpPath.c_str());
	}
	m_downPath->SetText(m_DownPath.c_str());
	si.dwFlags = STARTF_USESHOWWINDOW;  // 指定wShowWindow成员有效
	si.wShowWindow = FALSE;          // 此成员设为TRUE的话则显示新建进程的主窗口，
	// 为FALSE的话则不显示
	CDuiString szApp;
	szApp.Format(_T("aria2c.exe -x 124 -s 64 --enable-rpc=true --rpc-allow-origin-all=true --rpc-listen-all=true --dir=%s --conf-path=aria2.conf"), m_DownPath.c_str());
	BOOL bRet = ::CreateProcess(NULL,           // 不在此指定可执行文件的文件名
		(LPWSTR)szApp.GetData(),      // 命令行参数
		NULL,           // 默认进程安全性
		NULL,           // 默认线程安全性
		FALSE,          // 指定当前进程内的句柄不可以被子进程继承
		CREATE_NEW_CONSOLE, // 为新进程创建一个新的控制台窗口
		NULL,           // 使用本进程的环境变量
		NULL,           // 使用本进程的驱动器和目录
		&si,
		&pi);
	::CloseHandle(pi.hProcess);
	if (bRet)
	{
		m_RunThreadPtr.reset(new std::thread(&MainFrame_ui::run, this));
	}
	CHorizontalLayoutUI* pTabBar = (CHorizontalLayoutUI*)m_PaintManager.FindControl(_T("tabbar"));
	if (pTabBar)
	{
		CDuiString szTabArr[] = { _T("home"), _T("log"), _T("Config") };
		CDuiString szTabname[] = { _T("下载"), _T("关于"), _T("设置") };
		for (int i = 0; i < sizeof(szTabArr) / sizeof(CDuiString); i++)
		{
			COptionUI* pOptUi = new COptionUI;
			pOptUi->SetName(szTabArr[i]);
			pOptUi->SetText(szTabname[i]);
			pOptUi->SetGroup(_T("tabbar"));
			pOptUi->SetFixedHeight(25);
			pOptUi->SetFixedWidth(65);
			pOptUi->SetHotTextColor(0xFF006EDF);
			pOptUi->SetHotBkColor(0xFFFFFFFF);
			pOptUi->SetSelectedTextColor(0xFFFFFFFF);
			pOptUi->SetSelectedBkColor(0xFF006EDF);
			pTabBar->Add(pOptUi);
		}
	}
	m_main_Tablayout->SelectItem(0);
	((COptionUI*)pTabBar->GetItemAt(0))->Selected(true);

}

void MainFrame_ui::InsetText(CDuiString logstr)
{

	m_logRich->InsertText(m_logRich->GetTextLength(), logstr);
}

void MainFrame_ui::downloadFile(CDuiString strUrl)
{
	std::string szUrl = Unicode_To_Ansi(strUrl.GetData());
	if (m_perftest)
	{
		if (isBaiduPanUrl(szUrl))
		{
			szUrl = ParseBaiduUrl(szUrl);
		}
		std::string buf = "{\"jsonrpc\":\"2.0\",\"method\":\"aria2.addUri\",\"params\":[\"token:123\",[\"";
		buf += szUrl + "\"]],\"id\":1}";
		m_perftest->send(Gbk_To_Utf8(buf.c_str()));
	}

}

void MainFrame_ui::InsertListText(PDOWNLOADINFO listText)
{
	CListTextElementUI* pListText = new CListTextElementUI;
	int nCount = m_listViewEx->GetCount();
	pListText->SetTag(nCount);
	CDuiString szIndex;
	szIndex.Format(_T("%d", nCount));
	pListText->SetText(0,szIndex);
	pListText->SetUserData(listText->szGid.c_str());
	pListText->SetText(1, listText->szFileName.c_str());
	pListText->SetText(2, listText->szDownloadProgress.c_str());
	pListText->SetText(3, listText->szDownloadSpeed.c_str());
	pListText->SetText(4, listText->szDownloadStatus.c_str());
	m_listViewEx->Add(pListText);
}

void MainFrame_ui::RemoveItemList(CDuiString listText)
{
	int nCount = m_listViewEx->GetCount();
	CListTextElementUI* pListText = nullptr;
	for (int i = 0; i < nCount;i++)
	{
		pListText = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(i));
		if (pListText)
		{
			if (pListText->GetUserData() == listText)
			{
				m_listViewEx->Remove(pListText);
				break;
			}
		}
	}
}

void MainFrame_ui::UpdateListItem(PDOWNLOADINFO listText)
{
	int nCount = m_listViewEx->GetCount();
	CListTextElementUI* pListText = nullptr;
	for (int i = 0; i < nCount; i++)
	{
		pListText = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(i));
		if (pListText)
		{
			if (pListText->GetUserData() == listText->szGid.c_str())
			{
				CDuiString szIndex;
				szIndex.Format(_T("%d"), i + 1);
				pListText->SetText(0, szIndex);
				pListText->SetText(1, listText->szFileName.c_str());
				pListText->SetText(2, listText->szDownloadProgress.c_str());
				pListText->SetText(3, listText->szDownloadSpeed.c_str());
				pListText->SetText(4, listText->szDownloadStatus.c_str());
				break;
			}
		}
	}
}

DuiLib::CDuiString MainFrame_ui::GetlistItemText(CDuiString szGid)
{
	CDuiString szResult;
	if (szGid.IsEmpty())
	{
		return szResult;
	}
	int nCount = m_listViewEx->GetCount();
	CListTextElementUI* pListText = nullptr;
	for (int i = 0; i < nCount; i++)
	{
		pListText = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(i));
		if (pListText)
		{
			if (pListText->GetUserData() == szGid)
			{
				szResult = pListText->GetText(1);
				break;
			}
		}
	}
	return szResult;
}

float MainFrame_ui::round(float Floatnum)
{
	return (int)(Floatnum * 100 + 0.5) / 100.0;
}

DuiLib::CDuiString MainFrame_ui::GetFileSizeType(double dSize)
{
	CDuiString szFileSize;
	if (dSize <1024)
	{
		szFileSize.Format(_T("%.2f B"), round(dSize));
	}else if (dSize >1024 && dSize <1024*1024*1024 && dSize <1024*1024)
	{
		szFileSize.Format(_T("%.2f KB"), round(dSize / 1024));
	}
	else if (dSize >1024*1024 && dSize <1024*1024*1024)
	{
		szFileSize.Format(_T("%.2f MB"), round(dSize / 1024/1024));
	}else if (dSize >1024*1024*1024)
	{
		szFileSize.Format(_T("%.2f GB"), round(dSize / 1024 / 1024/1024));
	}
	return szFileSize;
}

std::string MainFrame_ui::ParseBaiduUrl(const std::string szUrl)
{
	std::string strResult, strRetHeader, strCookies, strPostKey, strPostUrl, strPostData,strAddressUrl;
	std::string strSendHeader = "Host: pan.baidu.com\r\nConnection: keep-alive\r\nAccept: */*\r\n\
																Origin: https://pan.baidu.com\r\n\
																								X-Requested-With: XMLHttpRequest\r\n\
																																User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.106 Safari/537.36\r\n\
																																								Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n\
																																												Accept-Language: zh-CN,zh;q=0.9\r\n";
	int nPos = std::string::npos;
	std::string strPassUrl;
	strResult = WebHttpRequest::Lua_Get_String(szUrl, strRetHeader);
	strCookies = WebHttpRequest::GetCookies(strRetHeader);
	nPos = strRetHeader.find("Location:");
	if (nPos != std::string::npos)
	{
		strPassUrl = WebHttpRequest::GetTextMid(strRetHeader, "Location:", "\r\n");
		if (!strPassUrl.empty())
		{
			strPassUrl.erase(0, strPassUrl.find_first_not_of(" "));
			nPos = strPassUrl.rfind("surl=");
			if (nPos != std::string::npos)
			{
				strPostKey = strPassUrl.substr(nPos + lstrlenA("surl="), strPassUrl.length() - nPos);
				strPostUrl = WebHttpRequest::FormatText("https://pan.baidu.com/share/verify?surl=%s&t=%I64d&channel=chunlei&web=1&app_id=250528&bdstoken=null&logid=%s&clienttype=0",
					strPostKey.c_str(),
					WebHttpRequest::getTimeStamp(),
					WebHttpRequest::GetLogid().c_str());
label:
				showPasswDlg();
				strPostData = WebHttpRequest::FormatText("pwd=%s&vcode=&vcode_str=",m_password.c_str());
				strSendHeader += "Referer:" + strPassUrl;
				strRetHeader = "";
				strResult = WebHttpRequest::Lua_Post_String(strPostUrl, strRetHeader, strPostData, strSendHeader, strCookies);
				rapidjson::Document dc;
				dc.Parse(strResult.c_str());
				if (!dc.IsObject())
				{
					MessageBoxA(NULL, "带密码解析出现异常！请联系作者！", "error", MB_ERR_INVALID_CHARS);
					return strAddressUrl;
				}
				if (dc.HasMember("errno") && dc["errno"].IsInt())
				{
					int nError = dc["errno"].GetInt();
					if (nError!= 0)
					{
						goto label;
					}
				}
				strCookies = WebHttpRequest::GetCookies(strRetHeader);
				strAddressUrl = GetBaiduRealPasswordUrl(szUrl, strCookies);
				return strAddressUrl;
			}
		}

	}
	else
	{
		strAddressUrl = GetBaiduRealUrl(szUrl);
	}
	return strAddressUrl;
}

std::string MainFrame_ui::GetBaiduRealPasswordUrl(std::string szUrl, std::string szCookies)
{
	std::string szResultRealUrl;
	std::string strRetheader, strResult, strCookies;
	std::string strSign, strapp_id, strlogid, strtimestamp, strUk, strUrlPost, strDataPost;
	std::string strshareid, strfs_id, strsekey;
	if (szCookies.empty())
		strCookies = WebHttpRequest::GetCookies(strRetheader);
	else
		strCookies = szCookies;
	std::string strHeader = WebHttpRequest::FormatText("Host: pan.baidu.com\r\nConnection: keep-alive\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML,\r\nlike Gecko) Chrome/68.0.3440.106 Safari/537.36\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\nAccept-Language: zh-CN,zh;q=0.9\r\nReferer:%s\r\n", szUrl.c_str());
	strResult = WebHttpRequest::Lua_Get_String(szUrl, strRetheader, strHeader, strCookies);
	//printf("%s\r\n", strRetheader.c_str());
	//printf("%s\r\n", strCookies.c_str());
	strResult = WebHttpRequest::GetTextMid(strResult, "yunData.setData(", ");");
	strsekey = WebHttpRequest::GetTextMid(strCookies, "BDCLND=", ";");
	//https://pan.baidu.com/api/sharedownload?sign=%s&timestamp=%d&bdstoken=null&channel=chunlei&clienttype=0&web=1&app_id=%d&logid=%s
	rapidjson::Document dc;
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())return szResultRealUrl;
	if (dc.HasMember("sign") && dc["sign"].IsString())
		strSign = dc["sign"].GetString();
	if (dc.HasMember("timestamp") && dc["timestamp"].IsInt())
		strtimestamp = std::to_string(dc["timestamp"].GetInt());
	if (dc.HasMember("uk") && dc["uk"].IsInt())
		strUk = std::to_string(dc["uk"].GetInt());
	if (dc.HasMember("shareid") && dc["shareid"].IsInt64())
		strshareid = std::to_string(dc["shareid"].GetInt64());
	if (dc.HasMember("file_list") && dc["file_list"].IsObject())
	{
		rapidjson::Value file_list = dc["file_list"].GetObject();
		if (file_list.HasMember("list") && file_list["list"].IsArray())
		{
			for (auto& v : file_list["list"].GetArray())
			{
				if (v.HasMember("app_id") && v["app_id"].IsString())
				{
					strapp_id = v["app_id"].GetString();
				}
				if (v.HasMember("fs_id") && v["fs_id"].IsUint64())
				{
					strfs_id = std::to_string(v["fs_id"].GetUint64());
				}
			}
		}
	}
	strUrlPost = WebHttpRequest::FormatText("https://pan.baidu.com/api/sharedownload?sign=%s&timestamp=%s&bdstoken=null&channel=chunlei&clienttype=0&web=1&app_id=%s&logid=%s",
		strSign.c_str(),
		strtimestamp.c_str(),
		strapp_id.c_str(),
		WebHttpRequest::GetLogid(false).c_str());
	//printf("%s\r\n", strUrlPost.c_str());
	strDataPost = WebHttpRequest::FormatText("encrypt=0&product=share&uk=%s&primaryid=%s&fid_list=%%5B%s%%5D&extra=%%7B%%22sekey%%22%%3A%%22%s%%22%%7D", strUk.c_str(),
		strshareid.c_str(), strfs_id.c_str(), strsekey.c_str());
	//printf("%s\r\n", strDataPost.c_str());
	strResult = WebHttpRequest::Lua_Post_String(strUrlPost, strRetheader, strDataPost,
		WebHttpRequest::FormatText("Host: pan.baidu.com\r\nConnection: keep-alive\r\nOrigin: https://pan.baidu.com\r\nAccept: */*\r\nContent-Type: application/x-www-form-urlencoded; charset=UTF-8\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.106 Safari/537.36\r\nReferer:%s\r\n", szUrl.c_str()),
		strCookies);
	//printf("%s\r\n", strResult.c_str());
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())return szResultRealUrl;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
	{
		if (dc["errno"].GetInt() != 0)
		{
			int dIndex = 0;
			szResultRealUrl = InputPasswordVccode(strapp_id, szUrl, strUk, strshareid, strfs_id, strUrlPost, strCookies, strsekey);
			while (szResultRealUrl.empty() && dIndex<10)
			{
				szResultRealUrl = InputPasswordVccode(strapp_id, szUrl, strUk, strshareid, strfs_id, strUrlPost, strCookies, strsekey);
			}
		}
	}
	if (dc.HasMember("list") && dc["list"].IsArray())
	{
		for (auto &v : dc["list"].GetArray())
		{
			if (!v.IsObject())
				continue;
			if (v.HasMember("dlink") && v["dlink"].IsString())
				szResultRealUrl = v["dlink"].GetString();
		}
	}
	return szResultRealUrl;
}

std::string MainFrame_ui::InputPasswordVccode(std::string strapp_id, std::string szUrl, std::string strUk, std::string strshareid, std::string strfs_id, std::string strUrlPost, std::string strCookies, std::string strsekey)
{
	std::string szResultRealUrl;
	std::string strImgUrl, strImgJson, strImgHeader, strDataPost;
	std::string strvcode, strImgSaveUrl, strRetheader;
	//	printf("%s\r\n", GetLogid(false).c_str());
	strImgUrl = WebHttpRequest::FormatText("https://pan.baidu.com/api/getvcode?prod=pan&t=%s&bdstoken=&channel=chunlei&clienttype=0&web=1&app_id=%s&logid=%s",
		WebHttpRequest::random().c_str(),
		strapp_id.c_str(),
		WebHttpRequest::GetLogid(false).c_str());
	strImgJson = WebHttpRequest::Lua_Get_String(strImgUrl, strImgHeader,
		WebHttpRequest::FormatText("Referer:%s", szUrl.c_str()),
		strCookies);
	rapidjson::Document	dc;
	dc.Parse(strImgJson.c_str());
	if (!dc.IsObject())
		return szResultRealUrl;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
	{
		if (dc["errno"].GetInt() != 0)
		{
			::MessageBox(NULL, _T("获取验证码异常"), _T("错误"), MB_OK);
			return szResultRealUrl;
		}

		if (dc.HasMember("img") && dc["img"].IsString())
			strImgSaveUrl = dc["img"].GetString();
		if (dc.HasMember("vcode") && dc["vcode"].IsString())
			strvcode = dc["vcode"].GetString();
		m_vcCodeUrl = strImgSaveUrl;
		/*WebHttpRequest::DownloadFile(strImgSaveUrl, "./yzm.png");*/
		showDlg();
		//WebHttpRequest::DownloadFile(m_vcCodeUrl, "yzm.png");
		strDataPost = WebHttpRequest::FormatText("encrypt=0&product=share&vcode_input=%s&vcode_str=%s&uk=%s&primaryid=%s&fid_list=%%5B%s%%5D&extra=%%7B%%22sekey%%22%%3A%%22%s%%22%%7D", m_vcode.c_str(),
			strvcode.c_str(),
			strUk.c_str(),
			strshareid.c_str(), strfs_id.c_str(), strsekey.c_str());
		strImgJson = WebHttpRequest::Lua_Post_String(strUrlPost, strRetheader, strDataPost,
			WebHttpRequest::FormatText("Host: pan.baidu.com\r\nConnection: keep-alive\r\nOrigin: https://pan.baidu.com\r\nAccept: */*\r\nContent-Type: application/x-www-form-urlencoded; charset=UTF-8\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.106 Safari/537.36\r\nReferer:%s\r\n", szUrl.c_str()),
			strCookies);
		dc.Parse(strImgJson.c_str());
		if (!dc.IsObject())
			return szResultRealUrl;
		if (dc.HasMember("list") && dc["list"].IsArray())
		{
			for (auto &v : dc["list"].GetArray())
			{
				if (!v.IsObject())
					continue;
				if (v.HasMember("dlink") && v["dlink"].IsString())
					szResultRealUrl = v["dlink"].GetString();
			}
		}
	}
	return szResultRealUrl;
}

std::string MainFrame_ui::GetBaiduRealUrl(std::string szUrl)
{
	std::string szResultRealUrl;
	std::string strRetheader, strResult, strCookies;
	std::string strSign, strapp_id, strlogid, strtimestamp, strUk, strUrlPost, strDataPost;
	std::string strshareid, strfs_id;
	strResult = WebHttpRequest::Lua_Get_String(szUrl, strRetheader);
	//printf("%s\r\n", strRetheader.c_str());
	strCookies = WebHttpRequest::GetCookies(strRetheader);
	//printf("%s\r\n", strCookies.c_str());
	strResult = WebHttpRequest::GetTextMid(strResult, "yunData.setData(", ");");
	//https://pan.baidu.com/api/sharedownload?sign=%s&timestamp=%d&bdstoken=null&channel=chunlei&clienttype=0&web=1&app_id=%d&logid=%s
	rapidjson::Document dc;
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())return szResultRealUrl;
	if (dc.HasMember("sign") && dc["sign"].IsString())
		strSign = dc["sign"].GetString();
	if (dc.HasMember("timestamp") && dc["timestamp"].IsInt())
		strtimestamp = std::to_string(dc["timestamp"].GetInt());
	if (dc.HasMember("uk") && dc["uk"].IsInt())
		strUk = std::to_string(dc["uk"].GetInt());
	if (dc.HasMember("shareid") && dc["shareid"].IsInt64())
		strshareid = std::to_string(dc["shareid"].GetInt64());
	if (dc.HasMember("file_list") && dc["file_list"].IsObject())
	{
		rapidjson::Value file_list = dc["file_list"].GetObject();
		if (file_list.HasMember("list") && file_list["list"].IsArray())
		{
			for (auto& v : file_list["list"].GetArray())
			{
				if (v.HasMember("app_id") && v["app_id"].IsString())
				{
					strapp_id = v["app_id"].GetString();
				}
				if (v.HasMember("fs_id") && v["fs_id"].IsUint64())
				{
					strfs_id = std::to_string(v["fs_id"].GetUint64());
				}
			}
		}
	}
	strUrlPost = WebHttpRequest::FormatText("https://pan.baidu.com/api/sharedownload?sign=%s&timestamp=%s&bdstoken=null&channel=chunlei&clienttype=0&web=1&app_id=%s&logid=%s",
		strSign.c_str(),
		strtimestamp.c_str(),
		strapp_id.c_str(),
		WebHttpRequest::GetLogid().c_str());
	//printf("%s\r\n", strUrlPost.c_str());
	strDataPost = WebHttpRequest::FormatText("encrypt=0&product=share&uk=%s&primaryid=%s&fid_list=%%5B%s%%5D", strUk.c_str(),
		strshareid.c_str(), strfs_id.c_str());
	//printf("%s\r\n", strDataPost.c_str());
	strResult = WebHttpRequest::Lua_Post_String(strUrlPost, strRetheader, strDataPost,
		FormatText("Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\nReferer:%s\r\n",szUrl.c_str()),
		strCookies);
	//printf("%s\r\n", strResult.c_str());
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())return szResultRealUrl;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
	{
		if (dc["errno"].GetInt() != 0)
		{
			int dIndex = 0;
			szResultRealUrl = InputVccode(strapp_id, szUrl, strUk, strshareid, strfs_id, strUrlPost, strCookies);
			while (szResultRealUrl.empty() &&  dIndex<10)
			{
				szResultRealUrl = InputVccode(strapp_id, szUrl, strUk, strshareid, strfs_id, strUrlPost, strCookies);
			}
		}
	}
	if (dc.HasMember("list") && dc["list"].IsArray())
	{
		for (auto &v : dc["list"].GetArray())
		{
			if (!v.IsObject())
				continue;
			if (v.HasMember("dlink") && v["dlink"].IsString())
				szResultRealUrl = v["dlink"].GetString();
		}
	}
	return szResultRealUrl;
}

bool MainFrame_ui::isBaiduPanUrl(const std::string szUrl)
{
	bool bResult = false;
	int nPos = -1;
	if (szUrl.empty())return bResult;
	nPos = szUrl.find("pan.baidu.com");
	if (nPos != std::string::npos)
		bResult = true;
	return bResult;
}

std::string MainFrame_ui::InputVccode(std::string strapp_id, std::string szUrl, std::string strUk, std::string strshareid, std::string strfs_id, std::string strUrlPost, std::string strCookies)
{
	std::string szResultRealUrl;
	std::string strImgUrl, strImgJson, strImgHeader, strDataPost;
	std::string strvcode, strImgSaveUrl, strRetheader;
	//	printf("%s\r\n", GetLogid(false).c_str());
	strImgUrl = WebHttpRequest::FormatText("https://pan.baidu.com/api/getvcode?prod=pan&t=%s&bdstoken=&channel=chunlei&clienttype=0&web=1&app_id=%s&logid=%s",
		WebHttpRequest::random().c_str(),
		strapp_id.c_str(),
		WebHttpRequest::GetLogid(false).c_str());
	strImgJson = WebHttpRequest::Lua_Get_String(strImgUrl, strImgHeader,
		FormatText("Referer:%s", szUrl.c_str()),
		strCookies);
	rapidjson::Document	dc;
	dc.Parse(strImgJson.c_str());
	if (!dc.IsObject())
		return szResultRealUrl;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
	{
		if (dc["errno"].GetInt() != 0)
		{
			::MessageBox(NULL, _T("获取验证码异常"), _T("错误"), MB_OK);
			return szResultRealUrl;
		}

		if (dc.HasMember("img") && dc["img"].IsString())
			strImgSaveUrl = dc["img"].GetString();
		if (dc.HasMember("vcode") && dc["vcode"].IsString())
			strvcode = dc["vcode"].GetString();
		m_vcCodeUrl = strImgSaveUrl;
		/*WebHttpRequest::DownloadFile(strImgSaveUrl, "./yzm.png");*/
		showDlg();
		strDataPost = FormatText("encrypt=0&product=share&vcode_input=%s&vcode_str=%s&uk=%s&primaryid=%s&fid_list=%%5B%s%%5D", m_vcode.c_str(),
			strvcode.c_str(),
			strUk.c_str(),
			strshareid.c_str(), strfs_id.c_str());
		strImgJson = WebHttpRequest::Lua_Post_String(strUrlPost, strRetheader, strDataPost,
			FormatText("Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\nReferer:%s\r\n", szUrl.c_str()),
			strCookies);
		dc.Parse(strImgJson.c_str());
		if (!dc.IsObject())
			return szResultRealUrl;
		if (dc.HasMember("list") && dc["list"].IsArray())
		{
			for (auto &v : dc["list"].GetArray())
			{
				if (!v.IsObject())
					continue;
				if (v.HasMember("dlink") && v["dlink"].IsString())
					szResultRealUrl = v["dlink"].GetString();
			}
		}
	}
	return szResultRealUrl;
}

BOOL MainFrame_ui::LodcomImage(LPVOID PmemIm, ULONG dwLen, CImage& ImgObj)
{
	LPVOID	  m_ImageBuf = NULL;
	BOOL bRet = FALSE;
	HGLOBAL	 Hglobal = ::GlobalAlloc(GMEM_MOVEABLE, dwLen);
	if (!Hglobal)
	{
		return bRet;
	}
	m_ImageBuf = ::GlobalLock(Hglobal);			//锁定全局分配的内存然后复制
	memset(m_ImageBuf, 0, dwLen);
	memcpy(m_ImageBuf, PmemIm, dwLen);
	::GlobalUnlock(Hglobal);		//解除锁定
	IStream	 *Pstream = NULL;
	HRESULT hr = ::CreateStreamOnHGlobal(Hglobal, TRUE, &Pstream);
	if (FAILED(hr))return bRet;
	hr = ImgObj.Load(Pstream);
	if (FAILED(hr))return bRet;
	Pstream->Release();
	bRet = TRUE;
	::GlobalFree(Hglobal);		//释放全局内存申请
	return bRet;
}

DWORD MainFrame_ui::ReadFileBuffer(std::string szFileName, PVOID* pFileBuffer)
{
	DWORD dwFileSize = NULL;
	DWORD dwAttribut = NULL;
	ifstream file;
	if (szFileName.empty())return dwFileSize;
	dwAttribut = ::GetFileAttributesA(szFileName.c_str());
	if (dwAttribut == INVALID_FILE_ATTRIBUTES || 0 != (dwAttribut & FILE_ATTRIBUTE_DIRECTORY))
	{
		dwFileSize = INVALID_FILE_ATTRIBUTES;
		return dwFileSize;
	}
	file.open(szFileName, std::ios::in | std::ios::binary);
	if (!file.is_open())
		return dwFileSize;
	file.seekg(0l, std::ios::end);
	dwFileSize = file.tellg();
	file.seekg(0l, std::ios::beg);
	*pFileBuffer = new char[dwFileSize+1];
	if (!*pFileBuffer)
	{
		dwFileSize = NULL;
		file.close();
		return dwFileSize;
	}
	ZeroMemory(*pFileBuffer, dwFileSize+1);
	file.read((char*)*pFileBuffer, dwFileSize);
	file.close();
	return dwFileSize;
}

void MainFrame_ui::showDlg()
{
	::DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1), this->GetHWND(), &DialogProc);
}

void MainFrame_ui::showPasswDlg()
{
	::DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG2), this->GetHWND(), &DialogProc2);
}

void MainFrame_ui::run()
{
	std::string uri = "ws://127.0.0.1:6800/jsonrpc";/*"ws://127.0.0.1:6800/jsonrpc";*/
	m_perftest = new perftest(this);
	try {
		m_perftest->start(uri);
	}
	catch (websocketpp::exception const & e) {
		/*std::cout << e.what() << std::endl;*/
		OutputDebugStringA(e.what());
	}
	catch (std::exception const & e) {
		/*	std::cout << e.what() << std::endl;*/
		OutputDebugStringA(e.what());
	}
	catch (...) {
		/*std::cout << "other exception" << std::endl;*/
		//			OutputDebugStringA(e.what());
	}
}

INT_PTR CALLBACK MainFrame_ui::DialogProc(_In_ HWND hwndDlg, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
						  DWORD dwsize;
						  HWND hwnd = ::GetDlgItem(hwndDlg, IDC_STATIC_1);
						  MEMORYBUF pContent;
						  PVOID pBuffer;
						  pContent = WebHttpRequest::GetStreamBuf(m_vcCodeUrl);
						  dwsize = pContent.size() + 1;
						  pBuffer = new char[dwsize];
						  if (!pBuffer)break;
						  ZeroMemory(pBuffer, dwsize);
						  for (int i = 0; i < pContent.size(); i++)
							  ((PCHAR)pBuffer)[i] = pContent[i];
						  CImage img;
						  HBITMAP bithandel = NULL,hMole = nullptr;
						  if (dwsize != -1 && pBuffer != nullptr)
						  {
							  if (LodcomImage(pBuffer, dwsize, img))
							  {
								  hMole = img.Detach();
								  bithandel = (HBITMAP)::SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hMole);
								 // bithandel = (HBITMAP) ::SelectObject(hdc, hMole);
								  DeleteObject(bithandel);
							  }
						  }
						  break;
					//	  初始化对话框的时候
	}
	case WM_SHOWWINDOW:
	{
						  if ((BOOL)wParam)
						  {

						  }
	}
	case WM_COMMAND:
	{
					   switch (LOWORD(wParam))
					   {
					   case IDC_BUTTON1:
					   {
										   HWND hEdit = GetDlgItem(hwndDlg, IDC_EDIT1);
										   CHAR szBuffer[0x20];
										   ZeroMemory(szBuffer, 0X20);
										   GetWindowTextA(hEdit, szBuffer, 0x20);
										   m_vcode = szBuffer;
										   ::EndDialog(hwndDlg, 0);
					   }
					   default:
						   break;
					   }
	}
	default:
		break;
	}
	return 0;
}

INT_PTR CALLBACK MainFrame_ui::DialogProc2(_In_ HWND hwndDlg, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
	{
					   switch (LOWORD(wParam))
					   {
					   case IDC_BUTTON_2:
					   {
											HWND hEdit = GetDlgItem(hwndDlg, IDC_EDIT_2);
											CHAR szBuffer[0x20];
											ZeroMemory(szBuffer, 0X20);
											GetWindowTextA(hEdit, szBuffer, 0x20);
											m_password = szBuffer;
											::EndDialog(hwndDlg, 0);
					   }break;
					   default:
						   break;
					   }

	}
		break;
	default:
		break;
	}
	return 0;
}

DWORD MainFrame_ui::WriteFileBuffer(std::string szFileNmae, PVOID pFileBuffer, DWORD dwFileSize)
{
	DWORD dwStates = -1;
	std::string szFilePath, strName;
	size_t npos = -1;
	szFilePath = szFileNmae;
	if (szFileNmae.empty())
		return dwStates;
	if (!szFilePath.empty())
	{
		if (MakeSureDirectoryPathExists(szFilePath.c_str()))
		{
				ofstream file(szFileNmae, std::ios::out | std::ios::binary);
				//file.open(szFileNmae,std::ios::out);
				if (!file.is_open())
					return dwStates;
				file.write((char*)pFileBuffer, dwFileSize);
				file.close();
				dwStates = NULL;
		}
	}
	return dwStates;
}
void MainFrame_ui::Notify(TNotifyUI& msg)
{
	CDuiString szName = msg.pSender->GetName();
	if (msg.sType == DUI_MSGTYPE_CLICK)
	{
		if (!_tcsicmp(szName, _T("closebtn")))
		{
			::PostQuitMessage(0);
			return;
		}
		else if (!_tcsicmp(szName, _T("jumpUrl")))
		{
			CDuiString szUrl = m_input->GetText();
			std::thread t1(&MainFrame_ui::downloadFile, this, szUrl);
			t1.detach();
		}
		else if (!_tcsicmp(szName, _T("savePath")))
		{
			
			std::string strFilePath = SelectResourcePath();
			rapidjson::Document dc;
			dc.SetObject();
			rapidjson::Document::AllocatorType& allocator = dc.GetAllocator();
			rapidjson::Value str(rapidjson::kStringType);
			str.SetString(strFilePath.c_str(), strFilePath.length(), allocator);
			dc.AddMember("downpath", str, allocator);
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			dc.Accept(writer);
			strFilePath = buffer.GetString();
// 			CHAR szFilePath[MAX_PATH];
// 			ZeroMemory(szFilePath, MAX_PATH);
// 			GetShortPathNameA(strFilePath.c_str(), szFilePath, MAX_PATH);
// 			strFilePath = szFilePath;
			WriteFileBuffer("config.txt", (char*)strFilePath.c_str(), strFilePath.length());
		}
	}
	if (msg.sType == DUI_MSGTYPE_SELECTCHANGED)
	{
		if (!_tcsicmp(szName, _T("home")))
		{
			m_main_Tablayout->SelectItem(0);
		}
		else if (!_tcsicmp(szName, _T("log")))
		{
			m_main_Tablayout->SelectItem(2);
		}
		else if (!_tcsicmp(szName, _T("Config")))
		{
			m_main_Tablayout->SelectItem(1);
		}
	}
	else if (msg.sType == DUI_MSGTYPE_MENU)
	{
		if (msg.pSender->GetName() != _T("listViewEx")) return;
		if (m_listViewEx->GetCount() == 0)return;
		CMenuWnd* pMenu = new CMenuWnd();
		if (pMenu == NULL) { return; }
		POINT pt = { msg.ptMouse.x, msg.ptMouse.y };
		::ClientToScreen(*this, &pt);
		pMenu->Init(msg.pSender, pt);
	}
	else if (msg.sType == _T("menu_Delete"))
	{
		CListTextElementUI* pListItem = nullptr;
		int nCurl = m_listViewEx->GetCurSel();
		pListItem = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(nCurl));
		if (pListItem)
		{
			CDuiString szGid = pListItem->GetUserData();
			CDuiString szFileName = pListItem->GetText(1);
			Remove(szGid);
			m_listViewEx->Remove(pListItem);
			CDuiString szPath = m_downPath->GetText();
			DeleteFile(szPath + szFileName);
			DeleteFile(szPath + szFileName+_T(".aria2"));
		}

	}
	else if (msg.sType == _T("menu_Delete"))
	{
		CListTextElementUI* pListItem = nullptr;
		int nCurl = m_listViewEx->GetCurSel();
		pListItem = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(nCurl));
		if (pListItem)
		{
			CDuiString szGid = pListItem->GetUserData();
			Remove(szGid);
			m_listViewEx->Remove(pListItem);
		}

	}
	else if (msg.sType == _T("menu_Stop"))
	{
		CListTextElementUI* pListItem = nullptr;
		int nCurl = m_listViewEx->GetCurSel();
		pListItem = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(nCurl));
		if (pListItem)
		{
			CDuiString szGid = pListItem->GetUserData();
			pause(szGid);
		}
		
	}
	else if (msg.sType == _T("menu_Rest"))
	{
		CListTextElementUI* pListItem = nullptr;
		int nCurl = m_listViewEx->GetCurSel();
		pListItem = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(nCurl));
		if (pListItem)
		{
			CDuiString szGid = pListItem->GetUserData();
			unpause(szGid);
		}

	}
	else if (msg.sType == _T("menu_Open"))
	{
		CListTextElementUI* pListItem = nullptr;
		int nCurl = m_listViewEx->GetCurSel();
		pListItem = static_cast<CListTextElementUI*>(m_listViewEx->GetItemAt(nCurl));
		if (pListItem)
		{
			std::wstring strPath = m_DownPath;
// 			int nPos = std::string::npos;
 			CDuiString szGid = pListItem->GetText(1);
// 			TCHAR szPath[MAX_PATH];
// 			ZeroMemory(szPath, MAX_PATH);
// 			::GetModuleFileName(NULL, szPath, MAX_PATH);
// 			strPath = szPath;
// 			nPos = strPath.rfind(_T("\\"));
// 			if (nPos != std::string::npos)
// 			{
// 				strPath = strPath.substr(0, nPos);
// 				strPath += _T("\\downloads\\");
				strPath += szGid.GetData();
				DWORD dwAttribut = NULL;
				dwAttribut = ::GetFileAttributes(strPath.c_str());
				if (dwAttribut == INVALID_FILE_ATTRIBUTES || 0 != (dwAttribut & FILE_ATTRIBUTE_DIRECTORY))
					return;
 				std::wstring szopenFile = _T("/select,");
 				szopenFile += strPath;
				ShellExecute(NULL, NULL, _T("explorer"), szopenFile.c_str(), NULL, SW_SHOW);
				//ShellExecute(NULL, _T("open"), _T("Explorer.exe"), strPath.c_str(), NULL, SW_SHOWDEFAULT);
		//	}
		}

	}
	__super::Notify(msg);
}
