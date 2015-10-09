#include "listkey.h"
#include <qredisclient/connection.h>

ListKeyModel::ListKeyModel(QSharedPointer<RedisClient::Connection> connection, QString fullPath, int dbIndex, long long ttl)
    : ListLikeKeyModel(connection, fullPath, dbIndex, ttl,
                       "LLEN", QByteArray(), "LRANGE", true)
{
}

QString ListKeyModel::getType()
{
    return "list";
}

void ListKeyModel::updateRow(int rowIndex, const QVariantMap &row)
{
    if (!isRowLoaded(rowIndex) || !isRowValid(row))
        throw Exception("Invalid row");

    if (isActualPositionChanged(rowIndex))
        throw Exception("Can't delete row from list, because row already has changed."
                        " Reload values and try again.");

    QByteArray newRow(row["value"].toByteArray());
    setListRow(rowIndex, newRow);
    m_rowsCache.replace(rowIndex,newRow);
}

void ListKeyModel::addRow(const QVariantMap &row)
{
    if (!isRowValid(row))
        throw Exception("Invalid row");

    addListRow(row["value"].toByteArray());
    m_rowCount++;
}

void ListKeyModel::removeRow(int i)
{
    if (!isRowLoaded(i))
        return;

    if (isActualPositionChanged(i))
        throw Exception("Can't delete row from list, because row already has changed."
                        " Reload values and try again.");

    // Replace value by system string
    QString customSystemValue("---VALUE_REMOVED_BY_RDM---");        
    setListRow(i, customSystemValue.toUtf8());

    // Remove all system values from list
    deleteListRow(0, customSystemValue.toUtf8());

    m_rowCount--;
    m_rowsCache.removeAt(i);

    if (m_rowCount == 0) {
        m_isKeyRemoved = true;
        m_notifier->removed();
    }
}

bool ListKeyModel::isActualPositionChanged(int row)
{
    using namespace RedisClient;

    QByteArray cachedValue = m_rowsCache[row];

    // check position update
    Response result;

    try {
        result = m_connection->commandSync("LRANGE", m_keyFullPath, QString::number(row),
                                           QString::number(row), m_dbIndex);
    } catch (const RedisClient::Connection::Exception& e) {
        throw Exception("Connection error: " + QString(e.what()));
    }

    QVariantList currentState = result.getValue().toList();

    return currentState.size() != 1 || currentState[0].toByteArray() != QString(cachedValue);
}

void ListKeyModel::addListRow(const QByteArray &value)
{
    try {
        m_connection->commandSync("LPUSH", m_keyFullPath, value, m_dbIndex);
    } catch (const RedisClient::Connection::Exception& e) {
        throw Exception("Connection error: " + QString(e.what()));
    }
}

void ListKeyModel::setListRow(int pos, const QByteArray &value)
{    
    try {
        m_connection->commandSync("LSET", m_keyFullPath,
                                  QString::number(pos), value, m_dbIndex);
    } catch (const RedisClient::Connection::Exception& e) {
        throw Exception("Connection error: " + QString(e.what()));
    }
}

void ListKeyModel::deleteListRow(int count, const QByteArray &value)
{       
    try {
        m_connection->commandSync("LREM", m_keyFullPath, QString::number(count),
                                  value, m_dbIndex);
    } catch (const RedisClient::Connection::Exception& e) {
        throw Exception("Connection error: " + QString(e.what()));
    }
}
