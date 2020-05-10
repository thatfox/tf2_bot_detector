#include "ActionManager.h"
#include "Actions.h"
#include "ConsoleLines.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <fstream>
#include <regex>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#undef min
#undef max

static const std::regex s_SingleCommandRegex(R"regex(((?:\w+)(?:\ +\w+)?)(?:\n)?)regex", std::regex::optimize);

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace tfbd_paths
{
	namespace local
	{
		static std::filesystem::path cfg()
		{
			return "tf2_bot_detector";
		}
		static std::filesystem::path cfg_temp()
		{
			return cfg() / "temp";
		}
	}
	namespace absolute
	{
		static std::filesystem::path root()
		{
			return "C:/Program Files (x86)/Steam/steamapps/common/Team Fortress 2/tf/custom/tf2_bot_detector/";
		}
		static std::filesystem::path cfg()
		{
			return root() / "cfg" / local::cfg();
		}
		static std::filesystem::path cfg_temp()
		{
			return root() / "cfg" / local::cfg_temp();
		}
	}
}

ActionManager::ActionManager()
{
	std::filesystem::remove_all(tfbd_paths::absolute::cfg_temp());
}

ActionManager::~ActionManager()
{
}

bool ActionManager::QueueAction(std::unique_ptr<IAction>&& action)
{
#if 0
	static uint32_t s_LastAction = 0;
	const uint32_t curAction = ++s_LastAction;
	{
		std::ofstream file(s_UpdateCFGPath / (""s << curAction << ".cfg"), std::ios_base::trunc);
		action->WriteCommands(file);
	}
	SendCommandToGame("+exec tf2_bot_detector/temp/"s << curAction << ".cfg");
#endif

	if (const auto maxQueuedCount = action->GetMaxQueuedCount();
		maxQueuedCount <= m_Actions.size())
	{
		const ActionType curActionType = action->GetType();
		size_t count = 0;
		for (const auto& queuedAction : m_Actions)
		{
			if (queuedAction->GetType() == curActionType)
			{
				if (++count >= maxQueuedCount)
					return false;
			}
		}
	}

	m_Actions.push_back(std::move(action));
	return true;
}

void ActionManager::Update(time_point_t curTime)
{
	if (curTime < (m_LastUpdateTime + UPDATE_INTERVAL))
		return;

	if (!m_Actions.empty())
	{
		bool actionTypes[(int)ActionType::COUNT]{};

		struct Writer final : IActionCommandWriter
		{
			void Write(std::string cmd, std::string args) override
			{
				assert(!cmd.empty());

				if (cmd.empty() && !args.empty())
					throw std::runtime_error("Empty command with non-empty args");

				if (!std::all_of(cmd.begin(), cmd.end(), [](char c) { return c == '_' || isalpha(c) || isdigit(c); }))
					throw std::runtime_error("Command contains invalid characters");

				if (!m_ComplexCommands && !std::all_of(args.begin(), args.end(), [](char c) { return isalpha(c) || isdigit(c) || isspace(c); }))
					m_ComplexCommands = true;

				m_Commands.push_back({ std::move(cmd), std::move(args) });
			}

			std::vector<std::pair<std::string, std::string>> m_Commands;
			bool m_ComplexCommands = false;

		} writer;

		for (auto it = m_Actions.begin(); it != m_Actions.end(); )
		{
			const IAction* action = it->get();
			const ActionType type = action->GetType();
			{
				auto& previousMsg = actionTypes[(int)type];
				const auto minInterval = action->GetMinInterval();

				if (minInterval.count() > 0 && (previousMsg || (curTime - m_LastTriggerTime[type]) < minInterval))
				{
					++it;
					continue;
				}

				previousMsg = true;
			}

			action->WriteCommands(writer);
			it = m_Actions.erase(it);
			m_LastTriggerTime[type] = curTime;
		}

		if (!writer.m_ComplexCommands)
		{
			std::string cmdLine;

			for (const auto& cmd : writer.m_Commands)
			{
				cmdLine << '+' << cmd.first << ' ';

				if (!cmd.second.empty())
					cmdLine << '"' << cmd.second << "\" ";
			}

			// A simple command, we can pass this directly to the engine
			SendCommandToGame(cmdLine);
		}
		else
		{
			// More complicated, write out a file and exec it
			const std::string cfgFilename = ""s << ++m_LastUpdateIndex << ".cfg";
			auto globalPath = tfbd_paths::absolute::cfg_temp();
			std::filesystem::create_directories(globalPath); // TODO: this is not ok for release
			globalPath /= cfgFilename;
			{
				std::ofstream file(globalPath, std::ios_base::trunc);
				assert(file.good());

				for (const auto& cmd : writer.m_Commands)
					file << cmd.first << ' ' << cmd.second << '\n';

				assert(file.good());
			}

			SendCommandToGame("+exec "s << (tfbd_paths::local::cfg_temp() / cfgFilename).generic_string());
		}
	}

	m_LastUpdateTime = curTime;
}

void ActionManager::SendCommandToGame(const std::string_view& cmd)
{
	if (cmd.empty())
		return;

	if (!FindWindowA("Valve001", nullptr))
	{
		Log("Attempted to send command \""s << cmd << "\" to game, but game is not running", { 1, 1, 0.8f });
		return;
	}

	static constexpr const char HL2_DIR[] = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\";

	std::string cmdLine;
	cmdLine << '"' << HL2_DIR << "hl2.exe\" -game tf -hijack " << cmd;

	STARTUPINFOA si{};
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi{};

	auto result = CreateProcessA(
		nullptr,             // application/module name
		cmdLine.data(),      // command line
		nullptr,             // process attributes
		nullptr,             // thread attributes
		FALSE,               // inherit handles
		IDLE_PRIORITY_CLASS, // creation flags
		nullptr,             // environment
		HL2_DIR,             // working directory
		&si,                 // STARTUPINFO
		&pi                  // PROCESS_INFORMATION
		);

	if (!result)
	{
		const auto error = GetLastError();
		std::string msg = "Failed to send command to hl2.exe: CreateProcess returned "s
			<< result << ", GetLastError returned " << error;
		throw std::runtime_error(msg);
	}

	//WaitForSingleObject(pi.hProcess, INFINITE);

	// We will never need these, close them now
	if (!CloseHandle(pi.hProcess))
		throw std::runtime_error(__FUNCTION__ ": Failed to close process");
	if (!CloseHandle(pi.hThread))
		throw std::runtime_error(__FUNCTION__ ": Failed to close process thread");
}