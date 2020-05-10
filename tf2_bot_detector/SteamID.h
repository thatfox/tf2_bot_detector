#pragma once

#include <cassert>
#include <compare>
#include <functional>
#include <iosfwd>
#include <string_view>

namespace tf2_bot_detector
{
	enum class SteamAccountType : uint64_t
	{
		Invalid = 0,
		Individual = 1,
		Multiseat = 2,
		GameServer = 3,
		AnonGameServer = 4,
		Pending = 5,
		ContentServer = 6,
		Clan = 7,
		Chat = 8,
		P2PSuperSeeder = 9,
		AnonUser = 10,
	};

	enum class SteamAccountUniverse : uint64_t
	{
		Invalid = 0,
		Public = 1,
		Beta = 2,
		Internal = 3,
		Dev = 4,
	};

	enum class SteamAccountInstance : uint64_t
	{
		All = 0,
		Desktop = (1 << 0),
		Console = (1 << 1),
		Web = (1 << 2),
	};

	class SteamID final
	{
	public:
		constexpr SteamID() = default;
		explicit SteamID(const std::string_view& str);
		explicit constexpr SteamID(uint64_t id64) : ID64(id64) {}
		explicit constexpr SteamID(uint32_t id, SteamAccountInstance instance, SteamAccountType type, SteamAccountUniverse universe) :
			ID(id), Instance(instance), Type(type), Universe(universe)
		{
		}

		std::strong_ordering operator<=>(const SteamID& other) const { return ID64 <=> other.ID64; }

#if _MSC_VER == 1925
		// Workaround for broken spaceship operator==/!=
		bool operator==(const SteamID& other) const { return std::is_eq(*this <=> other); }
		bool operator!=(const SteamID& other) const { return std::is_neq(*this <=> other); }
#endif

		constexpr bool IsValid() const { return Type != SteamAccountType::Invalid && Universe != SteamAccountUniverse::Invalid; }
		std::string str() const;

		union
		{
			uint64_t ID64 = 0;
			struct
			{
				uint64_t ID : 32;
				SteamAccountInstance Instance : 20;
				SteamAccountType Type : 4;
				SteamAccountUniverse Universe : 8;
			};
			struct
			{
				uint32_t Lower32;
				uint32_t Upper32;
			};
		};
	};
	static_assert(sizeof(SteamID) == sizeof(uint64_t));
}

namespace std
{
	template<>
	struct hash<tf2_bot_detector::SteamID>
	{
		inline size_t operator()(const tf2_bot_detector::SteamID& id) const
		{
			return std::hash<uint64_t>{}(id.ID64);
		}
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::SteamID& id)
{
	using namespace tf2_bot_detector;

	char c;
	switch (id.Type)
	{
	case SteamAccountType::Individual:     c = 'U'; break;
	case SteamAccountType::Multiseat:      c = 'M'; break;
	case SteamAccountType::GameServer:     c = 'G'; break;
	case SteamAccountType::AnonGameServer: c = 'A'; break;
	case SteamAccountType::Pending:        c = 'P'; break;
	case SteamAccountType::ContentServer:  c = 'C'; break;
	case SteamAccountType::Clan:           c = 'g'; break;
	case SteamAccountType::AnonUser:       c = 'a'; break;
	case SteamAccountType::Chat:           c = 'c'; break;

	default:
		assert(!"Invalid value when serializing SteamID");
	case SteamAccountType::Invalid:        c = 'I'; break;
	}

	return os << '[' << c << ':' << static_cast<int>(id.Universe) << ':' << id.ID << ']';
}