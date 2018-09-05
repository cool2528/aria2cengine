#ifndef __CLIENT__HPP__
#define __CLIENT__HPP__
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <chrono>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#define  GB2312_ACP 936
{

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
			m_endpoint.set_socket_init_handler(bind(&type::on_socket_init, this, CLIENT::_1));
			//m_endpoint.set_tls_init_handler(bind(&type::on_tls_init,this,::_1));
			m_endpoint.set_message_handler(bind(&type::on_message, this, CLIENT::_1, CLIENT::_2));
			m_endpoint.set_open_handler(bind(&type::on_open, this, CLIENT::_1));
			m_endpoint.set_close_handler(bind(&type::on_close, this, CLIENT::_1));
			m_endpoint.set_fail_handler(bind(&type::on_fail, this, CLIENT::_1));
		}
		~perftest()
		{
			for (auto& v : m_ThreadPtr)
			{
				v->join();
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

			//con->set_proxy("http://humupdates.uchicago.edu:8443");

			m_endpoint.connect(con);

			// Start the ASIO io_service run loop
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
			//	send(Gbk_To_Utf8("{\"jsonrpc\":\"2.0\",\"method\":\"aria2.addUri\",\"params\":[\"token:123\",[\"http://drg.mqego.com/VMware-workstation-full.zip\"]],\"id\":1}"));
			//	send(Gbk_To_Utf8("{\"jsonrpc\":\"2.0\",\"method\":\"aria2.addUri\",\"params\":[\"token:123\",[\"https://dldir1.qq.com/qqfile/qq/QQ9.0.5/23816/QQ9.0.5.exe\"]],\"id\":1}"));
			////	ws_stop();
			//m_endpoint.send(hdl, "", websocketpp::frame::opcode::text);
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
						//std::cout << "文件下载完成！" << std::endl;
						strGidComplete += "---------------------下载完成\n";
						OutputDebugStringA(strGidComplete.c_str());
						m_FrameDlgPtr->InsetText(Ansi_To_Unicode(strGidComplete.c_str()).c_str());
						m_endpoint.close(hdl, websocketpp::close::status::going_away, "");
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
			rapidjson::Document dc;
			ULONGLONG totalLength, completedLength, downloadSpeed;
			std::string status, strOutText;
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
			}
			int nDownloadState = (((double)completedLength / (double)totalLength) * 100);

			strOutText = FormatText("当前下载进度:%%（%d）每秒下载速度：%d kb", nDownloadState, downloadSpeed / 1024);
			strOutText += "\n";
			OutputDebugStringA(strOutText.c_str());
			m_FrameDlgPtr->InsetText(Ansi_To_Unicode(strOutText.c_str()).c_str());
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


	// int main(int argc, char* argv[]) {
	// 	std::string uri = "ws://127.0.0.1:6800/jsonrpc";
	// 	perftest endpoint;
	// 	if (argc == 2) {
	// 		uri = argv[1];
	// 	}
	// 
	// 	try {
	// 		endpoint.start(uri);
	// 	}
	// 	catch (websocketpp::exception const & e) {
	// 		std::cout << e.what() << std::endl;
	// 	}
	// 	catch (std::exception const & e) {
	// 		std::cout << e.what() << std::endl;
	// 	}
	// 	catch (...) {
	// 		std::cout << "other exception" << std::endl;
	// 	}
	// 	system("pause");
	// 	return 0;
	// }
}
#endif