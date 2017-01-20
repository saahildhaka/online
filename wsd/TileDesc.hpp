/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_TILEDESC_HPP
#define INCLUDED_TILEDESC_HPP

#include <cassert>
#include <map>
#include <sstream>
#include <string>

#include <Poco/StringTokenizer.h>

#include "Exceptions.hpp"
#include "Protocol.hpp"

/// Tile Descriptor
/// Represents a tile's coordinates and dimensions.
class TileDesc
{
public:
    TileDesc(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight, int ver, int imgSize, int id, bool broadcast) :
        _part(part),
        _width(width),
        _height(height),
        _tilePosX(tilePosX),
        _tilePosY(tilePosY),
        _tileWidth(tileWidth),
        _tileHeight(tileHeight),
        _ver(ver),
        _imgSize(imgSize),
        _id(id),
        _broadcast(broadcast),
        _oldHash(0),
        _hash(0)
    {
        if (_part < 0 ||
            _width <= 0 ||
            _height <= 0 ||
            _tilePosX < 0 ||
            _tilePosY < 0 ||
            _tileWidth <= 0 ||
            _tileHeight <= 0 ||
            _imgSize < 0)
        {
            throw BadArgumentException("Invalid tile descriptor.");
        }
    }

    int getPart() const { return _part; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getTilePosX() const { return _tilePosX; }
    int getTilePosY() const { return _tilePosY; }
    int getTileWidth() const { return _tileWidth; }
    int getTileHeight() const { return _tileHeight; }
    int getVersion() const { return _ver; }
    void setVersion(const int ver) { _ver = ver; }
    int getImgSize() const { return _imgSize; }
    void setImgSize(const int imgSize) { _imgSize = imgSize; }
    int getId() const { return _id; }
    bool getBroadcast() const { return _broadcast; }
    void setOldHash(uint64_t hash) { _oldHash = hash; }
    uint64_t getOldHash() const { return _oldHash; }
    void setHash(uint64_t hash) { _hash = hash; }
    uint64_t getHash() const { return _hash; }

    bool operator==(const TileDesc& other) const
    {
        return _part == other._part &&
               _width == other._width &&
               _height == other._height &&
               _tilePosX == other._tilePosX &&
               _tilePosY == other._tilePosY &&
               _tileWidth == other._tileWidth &&
               _tileHeight == other._tileHeight &&
               _id == other._id &&
               _broadcast == other._broadcast;
    }

    bool intersectsWithRect(int x, int y, int w, int h) const
    {
        return x + w >= getTilePosX() &&
               x <= getTilePosX() + getTileWidth() &&
               y + h >= getTilePosY() &&
               y <= getTilePosY() + getTileHeight();
    }

    bool intersects(const TileDesc& other) const
    {
        return intersectsWithRect(other.getTilePosX(), other.getTilePosY(),
                                  other.getTileWidth(), other.getTileHeight());
    }

    bool isAdjacent(const TileDesc& other) const
    {
        if (other.getPart() != getPart() ||
            other.getWidth() != getWidth() ||
            other.getHeight() != getHeight() ||
            other.getTileWidth() != getTileWidth() ||
            other.getTileHeight() != getTileHeight())
        {
            return false;
        }

        return intersects(other);
    }

    bool onSameRow(const TileDesc& other) const
    {
        if (other.getPart() != getPart() ||
            other.getWidth() != getWidth() ||
            other.getHeight() != getHeight() ||
            other.getTileWidth() != getTileWidth() ||
            other.getTileHeight() != getTileHeight())
        {
            return false;
        }

        return other.getTilePosY() + other.getTileHeight() >= getTilePosY() &&
               other.getTilePosY() <= getTilePosY() + getTileHeight();
    }

    /// Serialize this instance into a string.
    /// Optionally prepend a prefix.
    std::string serialize(const std::string& prefix = "") const
    {
        std::ostringstream oss;
        oss << prefix
            << " part=" << _part
            << " width=" << _width
            << " height=" << _height
            << " tileposx=" << _tilePosX
            << " tileposy=" << _tilePosY
            << " tilewidth=" << _tileWidth
            << " tileheight=" << _tileHeight
            << " oldhash=" << _oldHash
            << " hash=" << _hash;

        // Anything after ver is optional.
        oss << " ver=" << _ver;

        if (_id >= 0)
        {
            oss << " id=" << _id;
        }

        if (_imgSize > 0)
        {
            oss << " imgsize=" << _imgSize;
        }

        if (_broadcast)
        {
            oss << " broadcast=yes";
        }

        return oss.str();
    }

    /// Deserialize a TileDesc from a tokenized string.
    static TileDesc parse(const std::vector<std::string>& tokens)
    {
        // We don't expect undocumented fields and
        // assume all values to be int.
        std::map<std::string, int> pairs;

        // Optional.
        pairs["ver"] = -1;
        pairs["imgsize"] = 0;
        pairs["id"] = -1;

        uint64_t oldHash = 0;
        uint64_t hash = 0;
        for (size_t i = 0; i < tokens.size(); ++i)
        {
            if (LOOLProtocol::getTokenUInt64(tokens[i], "oldhash", oldHash))
                ;
            else if (LOOLProtocol::getTokenUInt64(tokens[i], "hash", hash))
                ;
            else
            {
                std::string name;
                int value = -1;
                if (LOOLProtocol::parseNameIntegerPair(tokens[i], name, value))
                {
                    pairs[name] = value;
                }
            }
        }

        std::string s;
        const bool broadcast = (LOOLProtocol::getTokenString(tokens, "broadcast", s) &&
                                s == "yes");

        auto result = TileDesc(pairs["part"], pairs["width"], pairs["height"],
                               pairs["tileposx"], pairs["tileposy"],
                               pairs["tilewidth"], pairs["tileheight"],
                               pairs["ver"],
                               pairs["imgsize"], pairs["id"], broadcast);
        result.setOldHash(oldHash);
        result.setHash(hash);

        return result;
    }

    /// Deserialize a TileDesc from a string format.
    static TileDesc parse(const std::string& message)
    {
        return parse(LOOLProtocol::tokenize(message.data(), message.size()));
    }

private:
    int _part;
    int _width;
    int _height;
    int _tilePosX;
    int _tilePosY;
    int _tileWidth;
    int _tileHeight;
    int _ver; //< Versioning support.
    int _imgSize; //< Used for responses.
    int _id;
    bool _broadcast;
    uint64_t _oldHash;
    uint64_t _hash;
};

/// One or more tile header.
/// Used to request the rendering of multiple
/// tiles as well as the header of the response.
class TileCombined
{
private:
    TileCombined(int part, int width, int height,
                 const std::string& tilePositionsX, const std::string& tilePositionsY,
                 int tileWidth, int tileHeight, const std::string& vers,
                 const std::string& imgSizes, int id,
                 const std::string& oldHashes,
                 const std::string& hashes) :
        _part(part),
        _width(width),
        _height(height),
        _tileWidth(tileWidth),
        _tileHeight(tileHeight),
        _id(id)
    {
        if (_part < 0 ||
            _width <= 0 ||
            _height <= 0 ||
            _tileWidth <= 0 ||
            _tileHeight <= 0)
        {
            throw BadArgumentException("Invalid tilecombine descriptor.");
        }

        Poco::StringTokenizer positionXtokens(tilePositionsX, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        Poco::StringTokenizer positionYtokens(tilePositionsY, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        Poco::StringTokenizer imgSizeTokens(imgSizes, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        Poco::StringTokenizer verTokens(vers, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        Poco::StringTokenizer oldHashTokens(oldHashes, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
        Poco::StringTokenizer hashTokens(hashes, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);

        const auto numberOfPositions = positionXtokens.count();

        // check that the comma-separated strings have the same number of elements
        if (numberOfPositions != positionYtokens.count() ||
            (!imgSizes.empty() && numberOfPositions != imgSizeTokens.count()) ||
            (!vers.empty() && numberOfPositions != verTokens.count()) ||
            (!oldHashes.empty() && numberOfPositions != oldHashTokens.count()) ||
            (!hashes.empty() && numberOfPositions != hashTokens.count()))
        {
            throw BadArgumentException("Invalid tilecombine descriptor. Unequal number of tiles in parameters.");
        }

        for (size_t i = 0; i < numberOfPositions; ++i)
        {
            int x = 0;
            if (!LOOLProtocol::stringToInteger(positionXtokens[i], x))
            {
                throw BadArgumentException("Invalid 'tileposx' in tilecombine descriptor.");
            }

            int y = 0;
            if (!LOOLProtocol::stringToInteger(positionYtokens[i], y))
            {
                throw BadArgumentException("Invalid 'tileposy' in tilecombine descriptor.");
            }

            int imgSize = 0;
            if (imgSizeTokens.count() && !LOOLProtocol::stringToInteger(imgSizeTokens[i], imgSize))
            {
                throw BadArgumentException("Invalid 'imgsize' in tilecombine descriptor.");
            }

            int ver = -1;
            if (verTokens.count() && !verTokens[i].empty() && !LOOLProtocol::stringToInteger(verTokens[i], ver))
            {
                throw BadArgumentException("Invalid 'ver' in tilecombine descriptor.");
            }

            uint64_t oldHash = 0;
            if (oldHashTokens.count() && !LOOLProtocol::stringToUInt64(oldHashTokens[i], oldHash))
            {
                throw BadArgumentException("Invalid tilecombine descriptor.");
            }

            uint64_t hash = 0;
            if (hashTokens.count() && !LOOLProtocol::stringToUInt64(hashTokens[i], hash))
            {
                throw BadArgumentException("Invalid tilecombine descriptor.");
            }

            _tiles.emplace_back(_part, _width, _height, x, y, _tileWidth, _tileHeight, ver, imgSize, id, false);
            _tiles.back().setOldHash(oldHash);
            _tiles.back().setHash(hash);
        }
    }

public:
    int getPart() const { return _part; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getTileWidth() const { return _tileWidth; }
    int getTileHeight() const { return _tileHeight; }

    const std::vector<TileDesc>& getTiles() const { return _tiles; }
    std::vector<TileDesc>& getTiles() { return _tiles; }

    /// Serialize this instance into a string.
    /// Optionally prepend a prefix.
    std::string serialize(const std::string& prefix = "") const
    {
        std::ostringstream oss;
        oss << prefix
            << " part=" << _part
            << " width=" << _width
            << " height=" << _height
            << " tileposx=";
        for (const auto& tile : _tiles)
        {
            oss << tile.getTilePosX() << ',';
        }
        oss.seekp(-1, std::ios_base::cur); // Seek back over last comma, overwritten below.

        oss << " tileposy=";
        for (const auto& tile : _tiles)
        {
            oss << tile.getTilePosY() << ',';
        }
        oss.seekp(-1, std::ios_base::cur); // Ditto.

        oss << " imgsize=";
        for (const auto& tile : _tiles)
        {
            oss << tile.getImgSize() << ','; // Ditto.
        }
        oss.seekp(-1, std::ios_base::cur);

        oss << " tilewidth=" << _tileWidth
            << " tileheight=" << _tileHeight;

        oss << " ver=";
        for (const auto& tile : _tiles)
        {
            oss << tile.getVersion() << ',';
        }
        oss.seekp(-1, std::ios_base::cur); // Ditto.

        oss << " oldhash=";
        for (const auto& tile : _tiles)
        {
            oss << tile.getOldHash() << ',';
        }
        oss.seekp(-1, std::ios_base::cur); // Ditto

        oss << " hash=";
        for (const auto& tile : _tiles)
        {
            oss << tile.getHash() << ',';
        }
        oss.seekp(-1, std::ios_base::cur); // See beow.

        if (_id >= 0)
        {
            oss << " id=" << _id;
        }

        // Make sure we don't return a potential trailing comma that
        // we have seeked back over but not overwritten after all.
        return oss.str().substr(0, oss.tellp());
    }

    /// Deserialize a TileDesc from a tokenized string.
    static TileCombined parse(const std::vector<std::string>& tokens)
    {
        // We don't expect undocumented fields and
        // assume all values to be int.
        std::map<std::string, int> pairs;

        // Optional.
        pairs["id"] = -1;

        std::string tilePositionsX;
        std::string tilePositionsY;
        std::string imgSizes;
        std::string versions;
        std::string oldhashes;
        std::string hashes;

        for (const auto& token : tokens)
        {
            std::string name;
            std::string value;
            if (LOOLProtocol::parseNameValuePair(token, name, value))
            {
                if (name == "tileposx")
                {
                    tilePositionsX = value;
                }
                else if (name == "tileposy")
                {
                    tilePositionsY = value;
                }
                else if (name == "imgsize")
                {
                    imgSizes = value;
                }
                else if (name == "ver")
                {
                    versions = value;
                }
                else if (name == "oldhash")
                {
                    oldhashes = value;
                }
                else if (name == "hash")
                {
                    hashes = value;
                }
                else
                {
                    int v = 0;
                    if (LOOLProtocol::stringToInteger(value, v))
                    {
                        pairs[name] = v;
                    }
                }
            }
        }

        return TileCombined(pairs["part"], pairs["width"], pairs["height"],
                            tilePositionsX, tilePositionsY,
                            pairs["tilewidth"], pairs["tileheight"],
                            versions,
                            imgSizes, pairs["id"], oldhashes, hashes);
    }

    /// Deserialize a TileDesc from a string format.
    static TileCombined parse(const std::string& message)
    {
        return parse(LOOLProtocol::tokenize(message.data(), message.size()));
    }

    static TileCombined create(const std::vector<TileDesc>& tiles)
    {
        assert(!tiles.empty());

        std::ostringstream xs;
        std::ostringstream ys;
        std::ostringstream vers;
        std::ostringstream oldhs;
        std::ostringstream hs;

        for (const auto& tile : tiles)
        {
            xs << tile.getTilePosX() << ',';
            ys << tile.getTilePosY() << ',';
            vers << tile.getVersion() << ',';
            oldhs << tile.getOldHash() << ',';
            hs << tile.getHash() << ',';
        }

        vers.seekp(-1, std::ios_base::cur); // Remove last comma.
        return TileCombined(tiles[0].getPart(), tiles[0].getWidth(), tiles[0].getHeight(),
                            xs.str(), ys.str(), tiles[0].getTileWidth(), tiles[0].getTileHeight(),
                            vers.str(), "", -1, oldhs.str(), hs.str());
    }

private:
    std::vector<TileDesc> _tiles;
    int _part;
    int _width;
    int _height;
    int _tileWidth;
    int _tileHeight;
    int _id;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
