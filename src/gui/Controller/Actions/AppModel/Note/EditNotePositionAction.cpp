//
// Created by fluty on 2024/2/8.
//

#include "EditNotePositionAction.h"
EditNotePositionAction *EditNotePositionAction::build(DsNote *note, int deltaTick, int deltaKey,
                                                      DsSingingClip *clip) {
    auto a = new EditNotePositionAction;
    a->m_note = note;
    a->m_deltaTick = deltaTick;
    a->m_deltaKey = deltaKey;
    a->m_clip = clip;
    return a;
}
void EditNotePositionAction::execute() {
    m_clip->removeNote(m_note);
    m_note->setStart(m_note->start() + m_deltaTick);
    m_note->setKeyIndex(m_note->keyIndex() + m_deltaKey);
    m_clip->insertNote(m_note);
}
void EditNotePositionAction::undo() {
    m_clip->removeNote(m_note);
    m_note->setStart(m_note->start() - m_deltaTick);
    m_note->setKeyIndex(m_note->keyIndex() - m_deltaKey);
    m_clip->insertNote(m_note);
}