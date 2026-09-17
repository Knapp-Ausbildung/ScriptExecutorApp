#include "LoginData.hpp"
#include "qobject.h"
#include <QDebug>
#include <qglobal.h>

LoginData::LoginData(QObject *parent) : QObject(parent) {}

void LoginData::saveCredentials(const QString &ipadress,
                                const QString &password) {

  qDebug() << "wurde gesaved";
  m_ipadress = ipadress;
  m_password = password;
}