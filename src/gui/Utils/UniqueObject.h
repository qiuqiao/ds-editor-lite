//
// Created by fluty on 2024/2/3.
//

#ifndef UNIQUEOBJECT_H
#define UNIQUEOBJECT_H

#include "IdGenerator.h"

class UniqueObject {
public:
    UniqueObject() {
        m_id = IdGenerator::instance()->id();
    }
    UniqueObject(int id) : m_id(id) {
    }
    int id() const {
        return m_id;
    }

protected:
    int m_id;
};

#endif // UNIQUEOBJECT_H
