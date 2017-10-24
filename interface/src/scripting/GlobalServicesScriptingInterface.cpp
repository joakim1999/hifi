//
//  GlobalServicesScriptingInterface.cpp
//  interface/src/scripting
//
//  Created by Thijs Wenker on 9/10/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AccountManager.h"
#include "Application.h"
#include "DiscoverabilityManager.h"
#include "ResourceCache.h"

#include "GlobalServicesScriptingInterface.h"

GlobalServicesScriptingInterface::GlobalServicesScriptingInterface() {
    auto accountManager = DependencyManager::get<AccountManager>();
    connect(accountManager.data(), &AccountManager::usernameChanged, this, &GlobalServicesScriptingInterface::onUsernameChanged);
    connect(accountManager.data(), &AccountManager::logoutComplete, this, &GlobalServicesScriptingInterface::loggedOut);
    connect(accountManager.data(), &AccountManager::loginComplete, this, &GlobalServicesScriptingInterface::connected);

    _downloading = false;
    QTimer* checkDownloadTimer = new QTimer(this);
    connect(checkDownloadTimer, &QTimer::timeout, this, &GlobalServicesScriptingInterface::checkDownloadInfo);
    const int CHECK_DOWNLOAD_INTERVAL = MSECS_PER_SECOND / 2;
    checkDownloadTimer->start(CHECK_DOWNLOAD_INTERVAL);

    auto discoverabilityManager = DependencyManager::get<DiscoverabilityManager>();
    connect(discoverabilityManager.data(), &DiscoverabilityManager::discoverabilityModeChanged,
            this, &GlobalServicesScriptingInterface::discoverabilityModeChanged);

    _loggedIn = isLoggedIn();
    emit loggedInChanged(_loggedIn);
}

GlobalServicesScriptingInterface::~GlobalServicesScriptingInterface() {
    auto accountManager = DependencyManager::get<AccountManager>();
    disconnect(accountManager.data(), &AccountManager::usernameChanged, this, &GlobalServicesScriptingInterface::onUsernameChanged);
    disconnect(accountManager.data(), &AccountManager::logoutComplete, this, &GlobalServicesScriptingInterface::loggedOut);
    disconnect(accountManager.data(), &AccountManager::loginComplete, this, &GlobalServicesScriptingInterface::connected);
}

GlobalServicesScriptingInterface* GlobalServicesScriptingInterface::getInstance() {
    static GlobalServicesScriptingInterface sharedInstance;
    return &sharedInstance;
}

const QString& GlobalServicesScriptingInterface::getUsername() const {
    return DependencyManager::get<AccountManager>()->getAccountInfo().getUsername();
}

bool GlobalServicesScriptingInterface::isLoggedIn() {
    auto accountManager = DependencyManager::get<AccountManager>();
    return accountManager->isLoggedIn();
}

bool GlobalServicesScriptingInterface::checkAndSignalForAccessToken() {
    auto accountManager = DependencyManager::get<AccountManager>();
    return accountManager->checkAndSignalForAccessToken();
}

void GlobalServicesScriptingInterface::logOut() {
    auto accountManager = DependencyManager::get<AccountManager>();
    return accountManager->logout();
}

void GlobalServicesScriptingInterface::loggedOut() {
    emit GlobalServicesScriptingInterface::disconnected(QString("logout"));
}

QString GlobalServicesScriptingInterface::getFindableBy() const {
    auto discoverabilityManager = DependencyManager::get<DiscoverabilityManager>();
    return DiscoverabilityManager::findableByString(discoverabilityManager->getDiscoverabilityMode());
}

void GlobalServicesScriptingInterface::setFindableBy(const QString& discoverabilityMode) {
    auto discoverabilityManager = DependencyManager::get<DiscoverabilityManager>();
    if (discoverabilityMode.toLower() == "none") {
        discoverabilityManager->setDiscoverabilityMode(Discoverability::None);
    } else if (discoverabilityMode.toLower() == "friends") {
        discoverabilityManager->setDiscoverabilityMode(Discoverability::Friends);
    } else if (discoverabilityMode.toLower() == "connections") {
        discoverabilityManager->setDiscoverabilityMode(Discoverability::Connections);
    } else if (discoverabilityMode.toLower() == "all") {
        discoverabilityManager->setDiscoverabilityMode(Discoverability::All);
    } else {
        qDebug() << "GlobalServices setFindableBy called with an unrecognized value. Did not change discoverability.";
    }
}

void GlobalServicesScriptingInterface::discoverabilityModeChanged(Discoverability::Mode discoverabilityMode) {
    emit findableByChanged(DiscoverabilityManager::findableByString(discoverabilityMode));
}

void GlobalServicesScriptingInterface::onUsernameChanged(QString username) {
    _loggedIn = (username != QString());
    emit myUsernameChanged(username);
    emit loggedInChanged(_loggedIn);
}

DownloadInfoResult::DownloadInfoResult() :
    downloading(QList<float>()),
    pending(0.0f)
{
}

QScriptValue DownloadInfoResultToScriptValue(QScriptEngine* engine, const DownloadInfoResult& result) {
    QScriptValue object = engine->newObject();

    QScriptValue array = engine->newArray(result.downloading.count());
    for (int i = 0; i < result.downloading.count(); i += 1) {
        array.setProperty(i, result.downloading[i]);
    }

    object.setProperty("downloading", array);
    object.setProperty("pending", result.pending);
    return object;
}

void DownloadInfoResultFromScriptValue(const QScriptValue& object, DownloadInfoResult& result) {
    QList<QVariant> downloading = object.property("downloading").toVariant().toList();
    result.downloading.clear();
    for (int i = 0; i < downloading.count(); i += 1) {
        result.downloading.append(downloading[i].toFloat());
    }

    result.pending = object.property("pending").toVariant().toFloat();
}

DownloadInfoResult GlobalServicesScriptingInterface::getDownloadInfo() {
    DownloadInfoResult result;
    foreach(const auto& resource, ResourceCache::getLoadingRequests()) {
        result.downloading.append(resource->getProgress() * 100.0f);
    }
    result.pending = ResourceCache::getPendingRequestCount();
    return result;
}

void GlobalServicesScriptingInterface::checkDownloadInfo() {
    DownloadInfoResult downloadInfo = getDownloadInfo();
    bool downloading = downloadInfo.downloading.count() > 0 || downloadInfo.pending > 0;

    // Emit signal if downloading or have just finished.
    if (downloading || _downloading) {
        _downloading = downloading;
        emit downloadInfoChanged(downloadInfo);
    }
}

void GlobalServicesScriptingInterface::updateDownloadInfo() {
    emit downloadInfoChanged(getDownloadInfo());
}

