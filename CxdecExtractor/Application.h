#pragma once

#include <string>
#include "ExtractCore.h"
namespace Engine
{
	using tTVPV2LinkProc = HRESULT(__stdcall*)(iTVPFunctionExporter*);
	using tTVPV2UnlinkProc = HRESULT(__stdcall*)();

	class Application
	{
	private:
		Application();
		Application(const Application&) = delete;
		Application(Application&&) = delete;
		Application& operator=(const Application&) = delete;
		Application& operator=(Application&&) = delete;
		~Application();

	private:
		HMODULE mModuleBase;				//dll基础地址
		std::wstring mDllDirectoryPath;		//dll目录
		std::wstring mCurrentDirectoryPath;	//游戏当前目录
		ExtractCore* mExtractor;			//解包器
		bool mTVPExporterInitialized;		//插件初始化成功标志

	public:

		/// <summary>
		/// 设置模块信息
		/// </summary>
		/// <param name="hModule">模块信息</param>
		void InitializeModule(HMODULE hModule);

		/// <summary>
		/// 初始化插件
		/// </summary>
		/// <param name="exporter">插件导出函数</param>
		void InitializeTVPEngine(iTVPFunctionExporter* exporter);

		/// <summary>
		/// 获取插件是否初始化完毕
		/// </summary>
		/// <returns>True已初始化 False未初始化</returns>
		bool IsTVPEngineInitialize();

		/// <summary>
		/// 获取解包器
		/// </summary>
		/// <returns></returns>
		ExtractCore* GetExtractor();

		/// <summary>
		/// 获取对象实例
		/// </summary>
		static Application* GetInstance();
		/// <summary>
		/// 初始化
		/// </summary>
		/// <param name="hModule">模块信息</param>
		static void Initialize(HMODULE hModule);
		/// <summary>
		/// 释放
		/// </summary>
		static void Release();
	};
}



