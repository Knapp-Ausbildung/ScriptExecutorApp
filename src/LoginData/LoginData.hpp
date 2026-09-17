#pragma once

#include "qobject.h"
#include "qtmetamacros.h"
#include <QObject>
#include <QString>

class LoginData : public QObject {
  Q_OBJECT

private:
  QString m_ipadress;
  QString m_password;

public:
  explicit LoginData(QObject *parent = nullptr);
  Q_INVOKABLE void saveCredentials(const QString &ipadress,
                                   const QString &password);
};