#pragma once

enum AssetTypeID
{
    ASSET_SPRITE_WHITE,
    ASSET_SPRITE_BALL,

    ASSET_COUNT
};

const char* get_asset(AssetTypeID id);
