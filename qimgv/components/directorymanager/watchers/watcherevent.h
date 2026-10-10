#pragma once

#include <QString>

class WatcherEvent {
public:
    enum Type {
        None,
        MovedFrom,
        MovedTo,
        Modify
    };

    WatcherEvent(const QString &name,int timerId, Type type = None);
    WatcherEvent(const QString& name, uint cookie, int timerId, Type type = None);
    ~WatcherEvent();

    QString name() const;
    void setName(const QString& name);

    uint cookie() const;

    int timerId() const;

    Type type() const;
    void setType(Type type);

private:
    QString mName;
    uint mCookie;
    int mTimerId;
    Type mType;

};
