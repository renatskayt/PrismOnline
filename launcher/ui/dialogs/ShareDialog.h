// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class MinecraftInstance;
class BaseInstance;

namespace Ui {
class ShareDialog;
}

class ShareDialog : public QDialog {
    Q_OBJECT
   public:
    explicit ShareDialog(QWidget* parent = nullptr);
    ~ShareDialog();

   private slots:
    void onShareInstance();
    void onUpdateInstance();
    void onDownloadInstance();
    void onUploadFinished(QNetworkReply*);
    void onUpdateFinished(QNetworkReply*);
    void onDownloadFinished(QNetworkReply*);
    void onInstanceChanged(int index);

   private:
    Ui::ShareDialog* ui;
    QNetworkAccessManager* m_network;
    QString m_pendingFile;
    bool m_isUpdate = false;
    MinecraftInstance* selectedInstance();
    QString getInstanceKey(MinecraftInstance* instance);
};
