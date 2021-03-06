#pragma once

#include "Actions/Actions.h"
#include "Actions/ActionManager.h"
#include "Clock.h"
#include "CompensatedTS.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "Networking/GithubAPI.h"
#include "ModeratorLogic.h"
#include "SetupFlow.h"
#include "WorldState.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "TFConstants.h"
#include "IConsoleLineListener.h"

#include <imgui_desktop/Window.h>

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct ImVec4;

namespace tf2_bot_detector
{
	class IConsoleLine;
	class IConsoleLineListener;

	class MainWindow final : public ImGuiDesktop::Window, IConsoleLineListener, BaseWorldEventListener
	{
	public:
		MainWindow();
		~MainWindow();

	private:
		void OnDraw() override;
		void OnDrawMenuBar() override;
		bool HasMenuBar() const override;
		void OnDrawScoreboard();
		void OnDrawScoreboardColorPicker(const char* name_id, float color[4]);
		void OnDrawScoreboardContextMenu(IPlayer& player);
		void OnDrawChat();
		void OnDrawAppLog();
		void OnDrawServerStats();

		void OnDrawSettingsPopup();
		bool m_SettingsPopupOpen = false;
		void OpenSettingsPopup() { m_SettingsPopupOpen = true; }

		void OnDrawUpdateCheckPopup();
		bool m_UpdateCheckPopupOpen = false;
		void OpenUpdateCheckPopup();

		void OnDrawUpdateAvailablePopup();
		bool m_UpdateAvailablePopupOpen = false;
		void OpenUpdateAvailablePopup();

		void GenerateDebugReport();

		GithubAPI::NewVersionResult* GetUpdateInfo();
		AsyncObject<GithubAPI::NewVersionResult> m_UpdateInfo;
		bool m_NotifyOnUpdateAvailable = true;

		void OnUpdate() override;

		bool IsSleepingEnabled() const override;

		bool IsTimeEven() const;
		float TimeSine(float interval = 1.0f, float min = 0, float max = 1) const;

		// IConsoleLineListener
		void OnConsoleLineParsed(WorldState& world, IConsoleLine& line) override;

		// IWorldEventListener
		//void OnChatMsg(WorldState& world, const IPlayer& player, const std::string_view& msg) override;
		void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;

		bool m_Paused = false;

		struct WorldStateExtra
		{
			WorldStateExtra(MainWindow& window, const Settings& settings, const std::filesystem::path& conLogFile);

			time_point_t m_LastStatusUpdateTime{};

			WorldState m_WorldState;
			ModeratorLogic m_ModeratorLogic;

			std::vector<const IConsoleLine*> m_PrintingLines;  // newest to oldest order
			static constexpr size_t MAX_PRINTING_LINES = 512;
			cppcoro::generator<IPlayer&> GeneratePlayerPrintData();
		};
		std::optional<WorldStateExtra> m_WorldState;
		bool IsWorldValid() const { return m_WorldState.has_value(); }
		WorldState& GetWorld() { return m_WorldState.value().m_WorldState; }
		const WorldState& GetWorld() const { return m_WorldState.value().m_WorldState; }
		ModeratorLogic& GetModLogic() { return m_WorldState.value().m_ModeratorLogic; }
		const ModeratorLogic& GetModLogic() const { return m_WorldState.value().m_ModeratorLogic; }

		// Gets the current timestamp, but time progresses in real time even without new messages
		time_point_t GetCurrentTimestampCompensated() const;

		struct PingSample
		{
			constexpr PingSample(time_point_t timestamp, uint16_t ping) :
				m_Timestamp(timestamp), m_Ping(ping)
			{
			}

			time_point_t m_Timestamp{};
			uint16_t m_Ping{};
		};

		struct PlayerExtraData final
		{
			PlayerExtraData(const IPlayer& player) : m_Parent(&player) {}

			const IPlayer* m_Parent = nullptr;

			time_point_t m_LastPingUpdateTime{};
			std::vector<PingSample> m_PingHistory{};
			float GetAveragePing() const;
		};

		struct EdictUsageSample
		{
			time_point_t m_Timestamp;
			uint16_t m_UsedEdicts;
			uint16_t m_MaxEdicts;
		};
		std::vector<EdictUsageSample> m_EdictUsageSamples;

		time_point_t m_OpenTime;

		void UpdateServerPing(time_point_t timestamp);
		std::vector<PingSample> m_ServerPingSamples;
		time_point_t m_LastServerPingSample{};

		Settings m_Settings;
		ActionManager m_ActionManager;
		SetupFlow m_SetupFlow;
	};
}
