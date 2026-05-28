// SPDX-License-Identifier: GPL-3.0-only
#include "ShareDialog.h"
#include "ui_ShareDialog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryFile>
#include <QUrlQuery>

#include "Application.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "InstanceImportTask.h"
#include "MMCZip.h"
#include "minecraft/MinecraftInstance.h"
#include "modplatform/modrinth/ModrinthPackExportTask.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "QObjectPtr.h"
#include "ui/dialogs/ProgressDialog.h"
#include "tasks/Task.h"

ShareDialog::ShareDialog(QWidget* parent) : QDialog(parent), ui(new Ui::ShareDialog), m_network(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    setWindowTitle(tr("PrismOnline — Pack Sharing"));

    connect(ui->shareButton, &QPushButton::clicked, this, &ShareDialog::onShareInstance);
    connect(ui->updateButton, &QPushButton::clicked, this, &ShareDialog::onUpdateInstance);
    connect(ui->downloadButton, &QPushButton::clicked, this, &ShareDialog::onDownloadInstance);
    connect(ui->instanceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShareDialog::onInstanceChanged);

    connect(m_network, &QNetworkAccessManager::finished, this, [this](QNetworkReply* reply) {
        if (m_isUpdate) {
            onUpdateFinished(reply);
        } else if (reply->operation() == QNetworkAccessManager::PostOperation) {
            onUploadFinished(reply);
        } else {
            onDownloadFinished(reply);
        }
    });

    // Populate instance list
    auto instances = APPLICATION->instances();
    for (int i = 0; i < instances->count(); i++) {
        auto inst = instances->at(i);
        ui->instanceCombo->addItem(inst->name(), inst->id());
    }
    if (ui->instanceCombo->count() > 0) {
        ui->instanceCombo->setCurrentIndex(0);
        onInstanceChanged(0);
    }
}

ShareDialog::~ShareDialog()
{
    delete ui;
}

MinecraftInstance* ShareDialog::selectedInstance()
{
    QString instId = ui->instanceCombo->currentData().toString();
    auto instances = APPLICATION->instances();
    for (int i = 0; i < instances->count(); i++) {
        auto inst = instances->at(i);
        if (inst->id() == instId)
            return dynamic_cast<MinecraftInstance*>(inst);
    }
    return nullptr;
}

QString ShareDialog::getInstanceKey(MinecraftInstance* instance)
{
    if (!instance)
        return {};
    return instance->settings()->get("PrismOnlineKey").toString();
}

void ShareDialog::onInstanceChanged(int index)
{
    Q_UNUSED(index);
    auto* instance = selectedInstance();
    QString key = getInstanceKey(instance);
    bool hasKey = !key.isEmpty();
    ui->updateButton->setEnabled(hasKey);
    if (hasKey) {
        ui->updateButton->setToolTip(tr("Push update to server (key: %1)").arg(key));
    } else {
        ui->updateButton->setToolTip(tr("This instance has no PrismOnline key. Share it first."));
    }
}

void ShareDialog::onShareInstance()
{
    auto* instance = selectedInstance();
    if (!instance) {
        QMessageBox::warning(this, tr("Error"), tr("No instance selected for sharing."));
        return;
    }

    // Create temp output file
    QTemporaryFile tmpFile(QDir::tempPath() + "/prismonline_share_XXXXXX.mrpack");
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create temporary file."));
        return;
    }
    QString outputPath = tmpFile.fileName();
    tmpFile.close();

    // Create and run export task
    auto exportTask = makeShared<ModrinthPackExportTask>(
        instance->name(), "1.0", "Shared via PrismOnline", false, instance, outputPath,
        nullptr, true);

    ProgressDialog progress(this);
    if (progress.execWithTask(exportTask.get()) != QDialog::Accepted) {
        QFile::remove(outputPath);
        return;
    }

    // Upload to server
    QFile file(outputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QFile::remove(outputPath);
        QMessageBox::critical(this, tr("Error"), tr("Failed to read pack file."));
        return;
    }
    QByteArray packData = file.readAll();
    file.close();

    QString serverUrl = APPLICATION->settings()->get("ShareServerURL").toString();
    QUrl url(serverUrl + "/api/share");
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    req.setRawHeader("X-Mrpack-Name", instance->name().toUtf8());
    req.setRawHeader("X-Mrpack-Icon", instance->iconKey().toUtf8());

    ui->statusLabel->setText(tr("Uploading pack to server..."));
    ui->shareButton->setEnabled(false);
    ui->updateButton->setEnabled(false);
    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(100);

    m_isUpdate = false;
    QNetworkReply* reply = m_network->post(req, packData);
    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            ui->progressBar->setMaximum(bytesTotal);
            ui->progressBar->setValue(bytesSent);
        }
    });

    // Store output path for cleanup
    m_pendingFile = outputPath;
}

void ShareDialog::onUpdateInstance()
{
    auto* instance = selectedInstance();
    if (!instance) {
        QMessageBox::warning(this, tr("Error"), tr("No instance selected."));
        return;
    }

    QString key = getInstanceKey(instance);
    if (key.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("This instance has no PrismOnline key. Share it first to get a key."));
        return;
    }

    // Create temp output file
    QTemporaryFile tmpFile(QDir::tempPath() + "/prismonline_update_XXXXXX.mrpack");
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create temporary file."));
        return;
    }
    QString outputPath = tmpFile.fileName();
    tmpFile.close();

    // Create and run export task
    auto exportTask = makeShared<ModrinthPackExportTask>(
        instance->name(), "1.0", "Shared via PrismOnline", false, instance, outputPath,
        nullptr, true);

    ProgressDialog progress(this);
    if (progress.execWithTask(exportTask.get()) != QDialog::Accepted) {
        QFile::remove(outputPath);
        return;
    }

    // Upload update to server via PUT
    QFile file(outputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QFile::remove(outputPath);
        QMessageBox::critical(this, tr("Error"), tr("Failed to read pack file."));
        return;
    }
    QByteArray packData = file.readAll();
    file.close();

    QString serverUrl = APPLICATION->settings()->get("ShareServerURL").toString();
    QUrl url(serverUrl + "/api/share/" + key);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    req.setRawHeader("X-Mrpack-Name", instance->name().toUtf8());
    req.setRawHeader("X-Mrpack-Icon", instance->iconKey().toUtf8());

    ui->statusLabel->setText(tr("Pushing update to server (key: %1)...").arg(key));
    ui->shareButton->setEnabled(false);
    ui->updateButton->setEnabled(false);
    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(100);

    m_isUpdate = true;
    QNetworkReply* reply = m_network->put(req, packData);
    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            ui->progressBar->setMaximum(bytesTotal);
            ui->progressBar->setValue(bytesSent);
        }
    });

    m_pendingFile = outputPath;
}

void ShareDialog::onDownloadInstance()
{
    QString key = ui->keyEdit->text().trimmed();
    if (key.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please enter a pack key."));
        return;
    }

    QString serverUrl = APPLICATION->settings()->get("ShareServerURL").toString();
    QUrl url(serverUrl + "/api/share/" + key);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    ui->statusLabel->setText(tr("Downloading pack..."));
    ui->downloadButton->setEnabled(false);
    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(100);

    m_isUpdate = false;
    QNetworkReply* reply = m_network->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            ui->progressBar->setMaximum(bytesTotal);
            ui->progressBar->setValue(bytesReceived);
        } else {
            ui->progressBar->setMaximum(0);
        }
    });
}

void ShareDialog::onUploadFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    ui->shareButton->setEnabled(true);
    onInstanceChanged(ui->instanceCombo->currentIndex());
    ui->progressBar->setVisible(false);

    // Cleanup temp file
    if (!m_pendingFile.isEmpty()) {
        QFile::remove(m_pendingFile);
        m_pendingFile.clear();
    }

    if (reply->error() != QNetworkReply::NoError) {
        ui->statusLabel->setText(tr("Upload failed"));
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to upload pack to server:\n%1").arg(reply->errorString()));
        return;
    }

    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject obj = doc.object();
    QString key = obj["key"].toString();
    int ttl = obj["ttl_days"].toInt();

    ui->statusLabel->setText(tr("Pack shared!"));
    QMessageBox::information(this, tr("Pack Key"),
                             tr("Pack uploaded successfully!\n\n"
                                "Share this key with friends:\n\n"
                                "%1\n\n"
                                "Key is valid for %2 days.")
                                 .arg(key)
                                 .arg(ttl));
}

void ShareDialog::onUpdateFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    m_isUpdate = false;
    ui->shareButton->setEnabled(true);
    onInstanceChanged(ui->instanceCombo->currentIndex());
    ui->progressBar->setVisible(false);

    // Cleanup temp file
    if (!m_pendingFile.isEmpty()) {
        QFile::remove(m_pendingFile);
        m_pendingFile.clear();
    }

    if (reply->error() != QNetworkReply::NoError) {
        ui->statusLabel->setText(tr("Update failed"));
        QString errorDetail;
        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            errorDetail = tr("Pack not found on server. The key may have expired.");
        } else {
            errorDetail = reply->errorString();
        }
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to push update to server:\n%1").arg(errorDetail));
        return;
    }

    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject obj = doc.object();
    QString key = obj["key"].toString();

    ui->statusLabel->setText(tr("Pack updated!"));
    QMessageBox::information(this, tr("Update Sent"),
                             tr("Pack update pushed successfully!\n\n"
                                "Key: %1\n\n"
                                "Friends with this key will get the update automatically.")
                                 .arg(key));
}

void ShareDialog::onDownloadFinished(QNetworkReply* reply)
{
    QString rawName = reply->rawHeader("X-Mrpack-Name");
    QString packName = QUrl::fromPercentEncoding(rawName.toUtf8());
    QString packIcon = reply->rawHeader("X-Mrpack-Icon");

    reply->deleteLater();
    ui->downloadButton->setEnabled(true);
    ui->progressBar->setVisible(false);

    if (reply->error() != QNetworkReply::NoError) {
        QString msg;
        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            msg = tr("Pack not found. Check the key.");
        } else if (reply->error() == QNetworkReply::OperationCanceledError) {
            msg = tr("Connection interrupted.");
        } else {
            msg = tr("Download error:\n%1").arg(reply->errorString());
        }
        ui->statusLabel->setText(tr("Error"));
        CustomMessageBox::selectable(this, tr("Error"), msg, QMessageBox::Warning)->show();
        return;
    }

    // Save to temp file
    QTemporaryFile tmpFile(QDir::tempPath() + "/prismonline_import_XXXXXX.mrpack");
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create temporary file."));
        return;
    }
    tmpFile.write(reply->readAll());
    QString packPath = tmpFile.fileName();
    tmpFile.close();

    ui->statusLabel->setText(tr("Importing pack..."));

    // Import as new instance
    auto* importTask = new InstanceImportTask(QUrl::fromLocalFile(packPath), this);
    importTask->setPrismOnlineKey(ui->keyEdit->text().trimmed());
    if (!packName.isEmpty()) {
        importTask->setName(packName);
    }
    if (!packIcon.isEmpty()) {
        importTask->setIcon(packIcon);
    } else {
        importTask->setIcon("modrinth");
    }

    unique_qobject_ptr<Task> wrapped_task(APPLICATION->instances()->wrapInstanceTask(importTask));

    ProgressDialog progress(this);
    if (progress.execWithTask(wrapped_task.get()) == QDialog::Accepted) {
        ui->statusLabel->setText(tr("Pack installed!"));
        QMessageBox::information(this, tr("Done"),
                                 tr("Pack imported successfully!\nIt will appear in the instance list."));
    } else {
        ui->statusLabel->setText(tr("Import failed"));
    }

    QFile::remove(packPath);
}
