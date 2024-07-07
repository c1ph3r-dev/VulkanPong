#include "assets.hpp"
#include "logger.hpp"
#include "platform.hpp"

const char* get_asset(AssetTypeID id){
   switch (id)
   {
   case ASSET_SPRITE_WHITE:
        {
            char* white = new char[4];
            white[0] = 255;
            white[1] = 255;
            white[2] = 255;
            white[3] = 255;
            return (const char*)white;
            break;
        }
    case ASSET_SPRITE_BALL:
        {
            uint32_t file_size;
            const char* data = platform_read_file((char*)"assets/textures/ball.DDS", &file_size);
            return data;
            break;
        }
    default:
        JONO_ASSERT(0, "Unrecognized AssetTypeID: %d", id);
        break;
   } 

   return nullptr;
}