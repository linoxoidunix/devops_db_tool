#pragma once
#include <QString>

struct ConnectionConfig {
    QString host     = "127.0.0.1";
    int     port     = 5432;
    QString database;
    QString user     = "postgres";
    QString password;
    QString schema   = "public";
};
