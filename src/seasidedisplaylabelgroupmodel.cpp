/*
 * Copyright (C) 2013 Jolla Mobile <bea.lam@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "seasidedisplaylabelgroupmodel.h"

#include <QContactStatusFlags>

#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>

#include <QDebug>

SeasideDisplayLabelGroupModel::SeasideDisplayLabelGroupModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_requiredProperty(NoPropertyRequired)
{
    SeasideCache::registerDisplayLabelGroupChangeListener(this);

    const QStringList &allGroups = SeasideCache::allDisplayLabelGroups();
    QHash<QString, QSet<quint32> > existingGroups = SeasideCache::displayLabelGroupMembers();
    if (!existingGroups.isEmpty()) {
        for (int i=0; i<allGroups.count(); i++)
            m_groups << SeasideDisplayLabelGroup(allGroups[i], existingGroups.value(allGroups[i]));
    } else {
        for (int i=0; i<allGroups.count(); i++)
            m_groups << SeasideDisplayLabelGroup(allGroups[i]);
    }
}

SeasideDisplayLabelGroupModel::~SeasideDisplayLabelGroupModel()
{
    SeasideCache::unregisterDisplayLabelGroupChangeListener(this);
}

int SeasideDisplayLabelGroupModel::requiredProperty() const
{
    return m_requiredProperty;
}

void SeasideDisplayLabelGroupModel::setRequiredProperty(int properties)
{
    if (m_requiredProperty != properties) {
        m_requiredProperty = properties;

        // Update counts
        QList<SeasideDisplayLabelGroup>::iterator it = m_groups.begin(), end = m_groups.end();
        for ( ; it != end; ++it) {
            SeasideDisplayLabelGroup &existing(*it);

            int newCount = countFilteredContacts(existing.contactIds);
            if (existing.count != newCount) {
                existing.count = newCount;

                const int row = it - m_groups.begin();
                const QModelIndex updateIndex(createIndex(row, 0));
                emit dataChanged(updateIndex, updateIndex);
            }
        }

        emit requiredPropertyChanged();
    }
}

QHash<int, QByteArray> SeasideDisplayLabelGroupModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(EntryCount, "entryCount");
    return roles;
}

int SeasideDisplayLabelGroupModel::rowCount(const QModelIndex &) const
{
    return m_groups.count();
}

QVariant SeasideDisplayLabelGroupModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
        case NameRole:
            return QString(m_groups[index.row()].name);
        case EntryCount:
            return m_groups[index.row()].count;
    }
    return QVariant();
}

void SeasideDisplayLabelGroupModel::displayLabelGroupsUpdated(const QHash<QString, QSet<quint32> > &groups)
{
    if (groups.isEmpty())
        return;

    bool wasEmpty = m_groups.isEmpty();
    bool insertedRows = false;

    if (wasEmpty) {
        const QStringList &allGroups = SeasideCache::allDisplayLabelGroups();
        if (!allGroups.isEmpty()) {
            beginInsertRows(QModelIndex(), 0, allGroups.count() - 1);
            for (int i=0; i<allGroups.count(); i++)
                m_groups << SeasideDisplayLabelGroup(allGroups[i]);

            insertedRows = true;
        }
    }

    QHash<QString, QSet<quint32> >::const_iterator it = groups.constBegin(), end = groups.constEnd();
    for ( ; it != end; ++it) {
        const QString group(it.key());

        QList<SeasideDisplayLabelGroup>::iterator existingIt = m_groups.begin(), existingEnd = m_groups.end();
        for ( ; existingIt != existingEnd; ++existingIt) {
            SeasideDisplayLabelGroup &existing(*existingIt);
            if (existing.name == group) {
                existing.contactIds = it.value();
                const int count = countFilteredContacts(existing.contactIds);
                if (existing.count != count) {
                    existing.count = count;
                    if (!wasEmpty) {
                        const int row = existingIt - m_groups.begin();
                        const QModelIndex updateIndex(createIndex(row, 0));
                        emit dataChanged(updateIndex, updateIndex);
                    }
                }
                break;
            }
        }
        if (existingIt == existingEnd) {
            // Find the index of this group in the groups list
            const QStringList &allGroups = SeasideCache::allDisplayLabelGroups();

            int allIndex = 0;
            int groupIndex = 0;
            for ( ; allIndex < allGroups.size() && allGroups.at(allIndex) != group; ++allIndex) {
                if (m_groups.at(groupIndex).name == allGroups.at(allIndex)) {
                    ++groupIndex;
                }
            }
            if (allIndex < allGroups.count()) {
                // Insert this group
                beginInsertRows(QModelIndex(), groupIndex, groupIndex);
                m_groups.insert(groupIndex, SeasideDisplayLabelGroup(group, it.value()));
                endInsertRows();

                insertedRows = true;
            } else {
                qWarning() << "Could not find unknown group in allGroups!" << group;
            }
        }
    }

    if (wasEmpty) {
        endInsertRows();
    }
    if (insertedRows) {
        emit countChanged();
    }
}

int SeasideDisplayLabelGroupModel::countFilteredContacts(const QSet<quint32> &contactIds) const
{
    if (m_requiredProperty != NoPropertyRequired) {
        int count = 0;

        // Check if these contacts are included by the current filter
        foreach (quint32 iid, contactIds) {
            if (SeasideCache::CacheItem *item = SeasideCache::existingItem(iid)) {
                bool haveMatch = (m_requiredProperty & AccountUriRequired) && (item->statusFlags & QContactStatusFlags::HasOnlineAccount);
                haveMatch |= (m_requiredProperty & PhoneNumberRequired) && (item->statusFlags & QContactStatusFlags::HasPhoneNumber);
                haveMatch |= (m_requiredProperty & EmailAddressRequired) && (item->statusFlags & QContactStatusFlags::HasEmailAddress);
                if (haveMatch)
                    ++count;
            } else {
                qWarning() << "SeasideDisplayLabelGroupModel: obsolete contact" << iid;
            }
        }

        return count;
    }

    return contactIds.count();
}

