/*
    RPG Paper Maker Copyright (C) 2017 Marie Laporte

    This file is part of RPG Paper Maker.

    RPG Paper Maker is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    RPG Paper Maker is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sprites.h"
#include "map.h"
#include "wanok.h"

// -------------------------------------------------------
//
//
//  ---------- SPRITESWALLS
//
//
// -------------------------------------------------------

// -------------------------------------------------------
//
//  CONSTRUCTOR / DESTRUCTOR / GET / SET
//
// -------------------------------------------------------

SpritesWalls::SpritesWalls() :
    m_count(0),
    m_vertexBuffer(QOpenGLBuffer::VertexBuffer),
    m_indexBuffer(QOpenGLBuffer::IndexBuffer),
    m_program(nullptr)
{

}

SpritesWalls::~SpritesWalls()
{

}

// -------------------------------------------------------
//
//  GL
//
// -------------------------------------------------------

void SpritesWalls::initializeVertices(GridPosition& position,
                                      SpriteWallDatas* sprite,
                                      int squareSize, int width, int height)
{
    sprite->initializeVertices(squareSize, width, height, m_vertices,
                                   m_indexes, position, m_count);
}

// -------------------------------------------------------

void SpritesWalls::initializeGL(QOpenGLShaderProgram* program) {
    if (m_program == nullptr){
        initializeOpenGLFunctions();

        // Programs
        m_program = program;
    }
}

// -------------------------------------------------------

void SpritesWalls::updateGL(){
    Map::updateGLStatic(m_vertexBuffer, m_indexBuffer, m_vertices, m_indexes,
                        m_vao, m_program);
}

// -------------------------------------------------------

void SpritesWalls::paintGL(){
    m_vao.bind();
    glDrawElements(GL_TRIANGLES, m_indexes.size(), GL_UNSIGNED_INT, 0);
    m_vao.release();
}

// -------------------------------------------------------
//
//
//  ---------- SPRITES
//
//
// -------------------------------------------------------

// -------------------------------------------------------
//
//  CONSTRUCTOR / DESTRUCTOR / GET / SET
//
// -------------------------------------------------------

Sprites::Sprites() :
    m_vertexBufferStatic(QOpenGLBuffer::VertexBuffer),
    m_indexBufferStatic(QOpenGLBuffer::IndexBuffer),
    m_programStatic(nullptr),
    m_vertexBufferFace(QOpenGLBuffer::VertexBuffer),
    m_indexBufferFace(QOpenGLBuffer::IndexBuffer),
    m_programFace(nullptr)
{

}

Sprites::~Sprites()
{
    QHash<Position, SpriteDatas*>::iterator i;
    for (i = m_all.begin(); i != m_all.end(); i++)
        delete *i;

    QHash<GridPosition, SpriteWallDatas*>::iterator j;
    for (j = m_walls.begin(); j != m_walls.end(); j++)
        delete *j;

    QHash<int, SpritesWalls*>::iterator k;
    for (k = m_wallsGL.begin(); k != m_wallsGL.end(); k++)
        delete *k;
}

void Sprites::addOverflow(Position& p) {
    m_overflow += p;
}

void Sprites::removeOverflow(Position& p) {
    m_overflow -= p;
}

// -------------------------------------------------------
//
//  INTERMEDIARY FUNCTIONS
//
// -------------------------------------------------------

bool Sprites::isEmpty() const{
    return m_all.size() == 0 && m_walls.size() == 0 && m_overflow.size() == 0;
}

// -------------------------------------------------------

bool Sprites::contains(Position& position) const {
    return m_all.contains(position);
}

// -------------------------------------------------------

SpriteDatas* Sprites::spriteAt(Position& position) const {
    return m_all.value(position);
}

// -------------------------------------------------------

void Sprites::setSprite(QSet<Portion>& portionsOverflow, Position& p,
                        SpriteDatas* sprite){
    m_all[p] = sprite;

    // Getting overflowing portions
    getSetPortionsOverflow(portionsOverflow, p, sprite);

    // Adding to overflowing
    addRemoveOverflow(portionsOverflow, p, true);
}

// -------------------------------------------------------

void Sprites::getSetPortionsOverflow(QSet<Portion>& portionsOverflow,
                                     Position& p, SpriteDatas* sprite)
{
    Portion currentPortion = Map::getGlobalPortion(p);
    int r = sprite->textureRect()->width() / 2;
    int h = sprite->textureRect()->height();

    for (int i = -r; i < r; i++) {
        for (int j = 0; j < h; j++) {
            for (int k = -r; k < r; k++) {
                Position newPosition = p;
                newPosition.addX(i);
                newPosition.addY(j);
                newPosition.addZ(k);
                Portion newPortion = Map::getGlobalPortion(newPosition);
                if (newPortion != currentPortion)
                    portionsOverflow += newPortion;
            }
        }
    }
}

// -------------------------------------------------------

void Sprites::addRemoveOverflow(QSet<Portion>& portionsOverflow, Position& p,
                                bool add)
{
    Map* map = Wanok::get()->project()->currentMap();
    for (QSet<Portion>::iterator i = portionsOverflow.begin();
         i != portionsOverflow.end(); i++)
    {
        Portion portion = *i;
        if (map->isPortionInGrid(portion)) {
            MapPortion* mapPortion = map->mapPortionFromGlobal(portion);
            bool write = false;
            if (mapPortion == nullptr) {
                write = true;
                mapPortion = map->loadPortionMap(portion.x(), portion.y(),
                                                 portion.z());
            }
            if (add)
                mapPortion->addOverflow(p);
            else
                mapPortion->removeOverflow(p);
            if (write) {
                map->savePortionMap(mapPortion);
                delete mapPortion;
            }
        }
        else {
            if (add)
                map->addOverflow(p, portion);
            else
                map->removeOverflow(p, portion);
        }
    }
}

// -------------------------------------------------------

SpriteDatas* Sprites::removeSprite(QSet<Portion>& portionsOverflow, Position& p)
{
    SpriteDatas* sprite = m_all.value(p);
    if (sprite != nullptr){
        m_all.remove(p);

        // Getting overflowing portions
        getSetPortionsOverflow(portionsOverflow, p, sprite);

        // Adding to overflowing
        addRemoveOverflow(portionsOverflow, p, false);

        return sprite;
    }

    return nullptr;
}

// -------------------------------------------------------

bool Sprites::addSprite(QSet<Portion>& portionsOverflow, Position& p,
                        MapEditorSubSelectionKind kind, int widthPosition,
                        int angle, QRect *textureRect)
{
    QSet<Portion> portionsOverflowRemove, portionsOverflowSet;
    SpriteDatas* previousSprite = removeSprite(portionsOverflowRemove, p);
    SpriteDatas* sprite = new SpriteDatas(kind, widthPosition, angle,
                                          textureRect);

    if (previousSprite != nullptr)
        delete previousSprite;

    setSprite(portionsOverflowSet, p, sprite);

    // Fusion of sets
    portionsOverflow.unite(portionsOverflowRemove);
    portionsOverflow.unite(portionsOverflowSet);

    return true;
}

// -------------------------------------------------------

bool Sprites::deleteSprite(QSet<Portion>& portionsOverflow, Position& p){
    SpriteDatas* previousSprite = removeSprite(portionsOverflow, p);

    if (previousSprite != nullptr)
        delete previousSprite;

    return true;
}

// -------------------------------------------------------

void Sprites::setSpriteWall(GridPosition& p, SpriteWallDatas* sprite) {
    m_walls[p] = sprite;
}

// -------------------------------------------------------

SpriteWallDatas* Sprites::removeSpriteWall(GridPosition &p) {
    SpriteWallDatas* sprite = m_walls.value(p);
    if (sprite != nullptr){
        m_walls.remove(p);
        return sprite;
    }

    return nullptr;
}

// -------------------------------------------------------

bool Sprites::addSpriteWall(GridPosition& p, int specialID) {
    SpriteWallDatas* previousSprite = removeSpriteWall(p);
    SpriteWallDatas* sprite = new SpriteWallDatas(specialID);

    if (previousSprite != nullptr)
        delete previousSprite;

    setSpriteWall(p, sprite);

    return true;
}

// -------------------------------------------------------

bool Sprites::deleteSpriteWall(GridPosition& p) {
    SpriteWallDatas* previousSprite = removeSpriteWall(p);

    if (previousSprite != nullptr)
        delete previousSprite;

    return true;
}

// -------------------------------------------------------

void Sprites::updateSpriteWalls(QHash<GridPosition, MapElement*>& previewGrid,
                                QList<GridPosition> &previewDeleteGrid) {
    QHash<GridPosition, SpriteWallDatas*> spritesWallWithPreview;
    getWallsWithPreview(spritesWallWithPreview, previewGrid, previewDeleteGrid);

    QHash<GridPosition, SpriteWallDatas*>::iterator i;
    for (i = spritesWallWithPreview.begin(); i != spritesWallWithPreview.end();
         i++)
    {
        GridPosition gridPosition = i.key();
        SpriteWallDatas* sprite = i.value();

        sprite->update(gridPosition);
    }
}

// -------------------------------------------------------

SpriteWallDatas* Sprites::getWallAt(QHash<GridPosition, MapElement*>&
                                    previewGrid,
                                    QList<GridPosition> &previewDeleteGrid,
                                    GridPosition& gridPosition)
{
    QHash<GridPosition, SpriteWallDatas*> spritesWallWithPreview;
    getWallsWithPreview(spritesWallWithPreview, previewGrid, previewDeleteGrid);

    return spritesWallWithPreview.value(gridPosition);
}

// -------------------------------------------------------

void Sprites::getWallsWithPreview(QHash<GridPosition, SpriteWallDatas *> &
                                  spritesWallWithPreview,
                                  QHash<GridPosition, MapElement *>&
                                  previewGrid,
                                  QList<GridPosition> &previewDeleteGrid)
{
    spritesWallWithPreview = m_walls;
    QHash<GridPosition, MapElement*>::iterator itw;
    for (itw = previewGrid.begin(); itw != previewGrid.end(); itw++) {
        MapElement* element = itw.value();
        if (element->getSubKind() == MapEditorSubSelectionKind::SpritesWall)
            spritesWallWithPreview[itw.key()] = (SpriteWallDatas*) element;
    }
    for (int i = 0; i < previewDeleteGrid.size(); i++) {
        spritesWallWithPreview.remove(previewDeleteGrid.at(i));
    }
}

// -------------------------------------------------------

void Sprites::removeSpritesOut(MapProperties& properties) {
    QList<Position> listGlobal;
    QList<GridPosition> listWalls;

    // Global sprites
    QHash<Position, SpriteDatas*>::iterator i;
    for (i = m_all.begin(); i != m_all.end(); i++) {
        Position position = i.key();

        if (position.x() >= properties.length() ||
            position.z() >= properties.width())
        {
            delete i.value();
            listGlobal.push_back(position);
        }
    }
    for (int k = 0; k < listGlobal.size(); k++)
        m_all.remove(listGlobal.at(k));

    // Walls sprites
    QHash<GridPosition, SpriteWallDatas*>::iterator j;
    for (j = m_walls.begin(); j != m_walls.end(); j++) {
        GridPosition gridPosition = j.key();

        if (gridPosition.x1() >= properties.length() ||
            gridPosition.z1() >= properties.width())
        {
            delete j.value();
            listWalls.push_back(gridPosition);
        }
    }
    for (int k = 0; k < listWalls.size(); k++)
        m_walls.remove(listWalls.at(k));
}

// -------------------------------------------------------

void Sprites::updateRaycasting(int squareSize, float &finalDistance,
                               Position& finalPosition, QRay3D &ray,
                               double cameraHAngle, int& spritesOffset)
{
    for (QHash<Position, SpriteDatas*>::iterator i = m_all.begin();
         i != m_all.end(); i++)
    {
        Position position = i.key();
        updateRaycastingAt(position, i.value(), squareSize, finalDistance,
                           finalPosition, ray, cameraHAngle, spritesOffset);
    }

    // Overflow
    Map* map = Wanok::get()->project()->currentMap();
    for (QSet<Position>::iterator i = m_overflow.begin();
         i != m_overflow.end(); i++)
    {
        Position position = *i;
        Portion portion = map->getLocalPortion(position);
        MapPortion* mapPortion = map->mapPortion(portion);
        mapPortion->updateRaycastingOverflowSprite(squareSize, position,
                                                   finalDistance, finalPosition,
                                                   ray, cameraHAngle);
    }
}

// -------------------------------------------------------

void Sprites::updateRaycastingAt(Position &position, SpriteDatas *sprite,
                                 int squareSize, float &finalDistance,
                                 Position &finalPosition, QRay3D& ray,
                                 double cameraHAngle, int& spritesOffset)
{
    float newDistance = sprite->intersection(squareSize, ray, position,
                                             cameraHAngle, spritesOffset);
    if (Wanok::getMinDistance(finalDistance, newDistance))
        finalPosition = position;
}

// -------------------------------------------------------
//
//  GL
//
// -------------------------------------------------------

void Sprites::initializeVertices(QHash<int, QOpenGLTexture *> &texturesWalls,
                                 QHash<Position, MapElement *> &previewSquares,
                                 QHash<GridPosition, MapElement *> &previewGrid,
                                 QList<GridPosition> &previewDeleteGrid,
                                 int squareSize, int width, int height,
                                 int& spritesOffset)
{
    int countStatic = 0;
    int countFace = 0;

    // Clear
    m_verticesStatic.clear();
    m_indexesStatic.clear();
    m_verticesFace.clear();
    m_indexesFace.clear();
    for (QHash<int, SpritesWalls*>::iterator i = m_wallsGL.begin();
         i != m_wallsGL.end(); i++)
    {
        delete *i;
    }
    m_wallsGL.clear();

    // Create temp hash for preview
    QHash<Position, SpriteDatas*> spritesWithPreview(m_all);
    for (QHash<Position, MapElement*>::iterator i = previewSquares.begin();
         i != previewSquares.end(); i++)
    {
        MapElement* element = i.value();
        if (element->getKind() == MapEditorSelectionKind::Sprites)
            spritesWithPreview[i.key()] = (SpriteDatas*) element;
    }
    QHash<GridPosition, SpriteWallDatas*> spritesWallWithPreview;
    getWallsWithPreview(spritesWallWithPreview, previewGrid, previewDeleteGrid);

    // Initialize vertices in squares
    for (QHash<Position, SpriteDatas*>::iterator i = spritesWithPreview.begin();
         i != spritesWithPreview.end(); i++)
    {
        Position position = i.key();
        SpriteDatas* sprite = i.value();

        sprite->initializeVertices(squareSize, width, height,
                                   m_verticesStatic, m_indexesStatic,
                                   m_verticesFace, m_indexesFace,
                                   position, countStatic, countFace,
                                   spritesOffset);
    }

    // Initialize vertices for walls
    for (QHash<GridPosition, SpriteWallDatas*>::iterator i =
         spritesWallWithPreview.begin(); i != spritesWallWithPreview.end(); i++)
    {
        GridPosition gridPosition = i.key();
        SpriteWallDatas* sprite = i.value();
        int id = sprite->wallID();
        SpritesWalls* sprites = m_wallsGL.value(id);
        if (sprites == nullptr) {
            sprites = new SpritesWalls;
            m_wallsGL[id] = sprites;
        }
        QOpenGLTexture* texture = texturesWalls.value(id);
        if (texture == nullptr)
            texture = texturesWalls.value(-1);

        sprites->initializeVertices(gridPosition, sprite, squareSize,
                                    texture->width(), texture->height());
    }
}

// -------------------------------------------------------

void Sprites::initializeGL(QOpenGLShaderProgram* programStatic,
                           QOpenGLShaderProgram *programFace){
    if (m_programStatic == nullptr){
        initializeOpenGLFunctions();

        // Programs
        m_programStatic = programStatic;
        m_programFace = programFace;
    }

    QHash<int, SpritesWalls*>::iterator i;
    for (i = m_wallsGL.begin(); i != m_wallsGL.end(); i++)
        i.value()->initializeGL(programStatic);
}

// -------------------------------------------------------

void Sprites::updateGL(){
    Map::updateGLStatic(m_vertexBufferStatic, m_indexBufferStatic,
                        m_verticesStatic, m_indexesStatic, m_vaoStatic,
                        m_programStatic);
    Map::updateGLFace(m_vertexBufferFace, m_indexBufferFace,
                      m_verticesFace, m_indexesFace, m_vaoFace,
                      m_programFace);
    QHash<int, SpritesWalls*>::iterator i;
    for (i = m_wallsGL.begin(); i != m_wallsGL.end(); i++)
        i.value()->updateGL();
}

// -------------------------------------------------------

void Sprites::paintGL(){
    m_vaoStatic.bind();
    glDrawElements(GL_TRIANGLES, m_indexesStatic.size(), GL_UNSIGNED_INT, 0);
    m_vaoStatic.release();
}

// -------------------------------------------------------

void Sprites::paintFaceGL(){
    m_vaoFace.bind();
    glDrawElements(GL_TRIANGLES, m_indexesFace.size(), GL_UNSIGNED_INT, 0);
    m_vaoFace.release();
}

// -------------------------------------------------------

void Sprites::paintSpritesWalls(int textureID) {
    SpritesWalls* sprites = m_wallsGL.value(textureID);
    if (sprites != nullptr)
        sprites->paintGL();
}

// -------------------------------------------------------
//
//  READ / WRITE
//
// -------------------------------------------------------

void Sprites::read(const QJsonObject & json){
    QJsonArray tab = json["list"].toArray();

    // Globals
    for (int i = 0; i < tab.size(); i++){
        QJsonObject obj = tab.at(i).toObject();
        Position p;
        p.read(obj["k"].toArray());
        QJsonObject objVal = obj["v"].toObject();
        SpriteDatas* sprite = new SpriteDatas;
        sprite->read(objVal);
        m_all[p] = sprite;
    }

    // Walls
    tab = json["walls"].toArray();
    for (int i = 0; i < tab.size(); i++){
        QJsonObject obj = tab.at(i).toObject();
        GridPosition p;
        p.read(obj["k"].toArray());
        QJsonObject objVal = obj["v"].toObject();
        SpriteWallDatas* sprite = new SpriteWallDatas;
        sprite->read(objVal);
        m_walls[p] = sprite;
    }

    // Overflow
    tab = json["overflow"].toArray();
    for (int i = 0; i < tab.size(); i++){
        QJsonArray tabPosition = tab.at(i).toArray();
        Position position;
        position.read(tabPosition);
        m_overflow += position;
    }
}

// -------------------------------------------------------

void Sprites::write(QJsonObject & json) const{
    QJsonArray tabGlobals, tabWalls, tabOverflow;

    // Globals
    for (QHash<Position, SpriteDatas*>::const_iterator i = m_all.begin();
         i != m_all.end(); i++)
    {
        QJsonObject objHash;
        QJsonArray tabKey;
        i.key().write(tabKey);
        QJsonObject objSprite;
        i.value()->write(objSprite);

        objHash["k"] = tabKey;
        objHash["v"] = objSprite;
        tabGlobals.append(objHash);
    }
    json["list"] = tabGlobals;

    // Walls
    for (QHash<GridPosition, SpriteWallDatas*>::const_iterator i =
         m_walls.begin(); i != m_walls.end(); i++)
    {
        QJsonObject objHash;
        QJsonArray tabKey;
        i.key().write(tabKey);
        QJsonObject objSprite;
        i.value()->write(objSprite);

        objHash["k"] = tabKey;
        objHash["v"] = objSprite;
        tabWalls.append(objHash);
    }
    json["walls"] = tabWalls;

    // Overflow
    for (QSet<Position>::const_iterator i = m_overflow.begin();
         i != m_overflow.end(); i++)
    {
        Position position = *i;
        QJsonArray tabPosition;
        position.write(tabPosition);
        tabOverflow.append(tabPosition);
    }
    json["overflow"] = tabOverflow;
}
