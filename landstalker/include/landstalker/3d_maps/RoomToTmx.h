#ifndef _ROOM_TO_TMX_H_
#define _ROOM_TO_TMX_H_

#include <string>
#include <landstalker/main/DataTypes.h>
#include <landstalker/main/GameData.h>

class RoomToTmx
{
public:
	static bool ExportToTmx(const std::string& fname, int roomnum, std::shared_ptr<GameData> gameData, const std::string& blockset_filename);
};

#endif // _ROOM_TO_TMX_H_