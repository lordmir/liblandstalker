#ifndef _ROOM_TO_YAML_H_
#define _ROOM_TO_YAML_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace YAML {
class Emitter;
}

namespace Landstalker {

class GameData;
class Room;

class RoomToYaml
{
public:
	using RoomKey = std::variant<std::nullopt_t, uint16_t, std::string>;

	static bool ExportToYaml(const std::string& filename, uint16_t roomnum,
		const std::shared_ptr<GameData>& game_data);
	static bool ImportFromYaml(const std::string& filename, const RoomKey& key,
		const std::shared_ptr<GameData>& game_data);
	static bool ImportFromYamlText(const std::string& yaml, const RoomKey& key,
		const std::shared_ptr<GameData>& game_data);
	static void EmitYaml(YAML::Emitter& out, const Room& room,
		const std::shared_ptr<GameData>& game_data);
};

} // namespace Landstalker

#endif // _ROOM_TO_YAML_H_
