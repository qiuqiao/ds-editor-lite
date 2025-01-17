//
// Created by hrukalive on 2/7/24.
//

#include "DspxProjectConverter.h"

#include "opendspx/qdspxtrack.h"
#include "opendspx/qdspxtimeline.h"
#include "opendspx/qdspxmodel.h"

#include <QMessageBox>

bool DspxProjectConverter::load(const QString &path, AppModel *model, QString &errMsg,
                             ImportMode mode) {
    auto decodeCurves = [&](const QList<QDspx::ParamCurveRef> &dspxCurveRefs) {
        QVector<DsCurve*> curves;
        for (const QDspx::ParamCurveRef &dspxCurveRef : dspxCurveRefs) {
            if (dspxCurveRef->type == QDspx::ParamCurve::Type::Free) {
                auto castCurveRef = dspxCurveRef.dynamicCast<QDspx::ParamFree>();
                auto curve = new DsDrawCurve;
                curve->setStart(castCurveRef->start);
                curve->step = castCurveRef->step;
                curve->setValues(castCurveRef->values);
                curves.append(curve);
            } else if (dspxCurveRef->type == QDspx::ParamCurve::Type::Anchor) {
                auto castCurveRef = dspxCurveRef.dynamicCast<QDspx::ParamAnchor>();
                auto curve = new DsAnchorCurve;
                curve->setStart(castCurveRef->start);
                for (const auto &dspxNode : castCurveRef->nodes) {
                    auto node = new DsAnchorNode(dspxNode.x, dspxNode.y);
                    node->setInterpMode(DsAnchorNode::None);
                    if (dspxNode.interp == QDspx::AnchorPoint::Interpolation::Linear) {
                        node->setInterpMode(DsAnchorNode::Linear);
                    } else if (dspxNode.interp == QDspx::AnchorPoint::Interpolation::Hermite) {
                        node->setInterpMode(DsAnchorNode::Hermite);
                    } /*else if (dspxNode.interp == QDspx::AnchorPoint::Interpolation::Cubic) {
                        node->setInterpMode(DsAnchorNode::Cubic);
                    }*/
                    curve->insertNode(node);
                }
                curves.append(curve);
            }
        }
        return curves;
    };

    auto decodeSingingParam = [&](const QDspx::ParamInfo &dspxParam) {
        DsParam param;
        for (auto &curve : decodeCurves(dspxParam.org)) {
            param.original.add(curve);
        }
        for (auto &curve : decodeCurves(dspxParam.edited)) {
            param.edited.add(curve);
        }
        for (auto &curve: decodeCurves(dspxParam.envelope)) {
            param.envelope.add(curve);
        }
        return param;
    };

    auto decodeSingingParams = [&](const QDspx::SingleParam &dspxParams) {
        DsParams params;
        params.pitch = decodeSingingParam(dspxParams.pitch);
        params.energy = decodeSingingParam(dspxParams.energy);
        params.tension = decodeSingingParam(dspxParams.tension);
        params.breathiness = decodeSingingParam(dspxParams.breathiness);
        return params;
    };

    auto decodePhonemes = [&](const QList<QDspx::Phoneme> &dspxPhonemes) {
        QList<DsPhoneme> phonemes;
        for (const QDspx::Phoneme &dspxPhoneme : dspxPhonemes) {
            DsPhoneme phoneme(DsPhoneme::DsPhonemeType::Normal, dspxPhoneme.token, dspxPhoneme.start);
            if (dspxPhoneme.type == QDspx::Phoneme::Type::Ahead) {
                phoneme.type = DsPhoneme::DsPhonemeType::Ahead;
            } else if (dspxPhoneme.type == QDspx::Phoneme::Type::Final) {
                phoneme.type = DsPhoneme::DsPhonemeType::Final;
            }
            phonemes.append(phoneme);
        }
        return phonemes;
    };
    auto decodeNotes = [&](const QList<QDspx::Note> &dspxNotes) {
        QList<DsNote *> notes;
        for (const QDspx::Note &dspxNote : dspxNotes) {
            auto note = new DsNote;
            note->setStart(dspxNote.pos);
            note->setLength(dspxNote.length);
            note->setKeyIndex(dspxNote.keyNum);
            note->setLyric(dspxNote.lyric);
            note->setPronunciation(dspxNote.pronunciation);
            note->setPhonemes(DsPhonemes::Original, decodePhonemes(dspxNote.phonemes.org));
            note->setPhonemes(DsPhonemes::Edited, decodePhonemes(dspxNote.phonemes.edited));
            notes.append(note);
        }
        return notes;
    };
    auto decodeClips = [&](const QList<QDspx::ClipRef> &dspxClips, DsTrack *track) {
        for (const auto &dspxClip : dspxClips) {
            if (dspxClip->type == QDspx::Clip::Type::Singing) {
                auto castClip = dspxClip.dynamicCast<QDspx::SingingClip>();
                auto clip = new DsSingingClip;
                clip->setName(castClip->name);
                clip->setStart(castClip->time.start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                auto notes = decodeNotes(castClip->notes);
                for (auto &note : notes)
                    clip->insertNoteQuietly(note);
                clip->params = decodeSingingParams(castClip->params);
                track->insertClipQuietly(clip);
            } else if (dspxClip->type == QDspx::Clip::Type::Audio) {
                auto castClip = dspxClip.dynamicCast<QDspx::AudioClip>();
                auto clip = new DsAudioClip;
                clip->setName(castClip->name);
                clip->setStart(castClip->time.start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                clip->setPath(castClip->path);
                track->insertClipQuietly(clip);
            }
        }
    };

    auto decodeTracks = [&](const QList<QDspx::Track> &dspxTracks, AppModel *model) {
        int i = 0;
        for (const auto &dspxTrack : dspxTracks) {
            auto track = new DsTrack;
            auto trackControl = DsTrackControl();
            trackControl.setGain(dspxTrack.control.gain);
            trackControl.setPan(dspxTrack.control.pan);
            trackControl.setMute(dspxTrack.control.mute);
            trackControl.setSolo(dspxTrack.control.solo);
            track->setName(dspxTrack.name);
            track->setControl(trackControl);
            decodeClips(dspxTrack.clips, track);
            model->insertTrackQuietly(track, i);
            i++;
        }
    };

    QDspxModel dspxModel;
    auto returnCode = dspxModel.load(path);
    if (returnCode.type == QDspx::ReturnCode::Success) {
        auto timeline = dspxModel.content.timeline;
        model->setTimeSignature(AppModel::TimeSignature(timeline.timeSignatures[0].num, timeline.timeSignatures[0].den));
        model->setTempo(timeline.tempos[0].value);
        decodeTracks(dspxModel.content.tracks, model);
        return true;
    } else {
        errMsg = QString("Failed to load project file.\r\npath: %1\r\ntype: %2 code: %3")
                     .arg(path)
                     .arg(returnCode.type)
                     .arg(returnCode.code);
        return false;
    }
}

bool DspxProjectConverter::save(const QString &path, AppModel *model, QString &errMsg) {

    auto encodeCurves = [&](const OverlapableSerialList<DsCurve> &dsCurves, QList<QDspx::ParamCurveRef> &curves) {
        for (const auto &dsCurve : dsCurves) {
            if (dsCurve->type() == DsCurve::DsCurveType::Draw) {
                auto castCurve = dynamic_cast<DsDrawCurve *>(dsCurve);
                auto curve = QDspx::ParamFreeRef::create();
                curve->start = castCurve->start();
                curve->step = castCurve->step;
                for (const auto v : castCurve->values()) {
                    curve->values.append(v);
                }
                curves.append(curve);
            } else if (dsCurve->type() == DsCurve::DsCurveType::Anchor) {
                auto castCurve = dynamic_cast<DsAnchorCurve *>(dsCurve);
                auto curve = QDspx::ParamAnchorRef::create();
                curve->start = dsCurve->start();
                for (const auto dsNode : castCurve->nodes()) {
                    QDspx::AnchorPoint node;
                    node.x = dsNode->pos();
                    node.y = dsNode->value();
                    if (dsNode->interpMode() == DsAnchorNode::None) {
                        node.interp = QDspx::AnchorPoint::Interpolation::None;
                    } else if (dsNode->interpMode() == DsAnchorNode::Linear) {
                        node.interp = QDspx::AnchorPoint::Interpolation::Linear;
                    } else if (dsNode->interpMode() == DsAnchorNode::Hermite) {
                        node.interp = QDspx::AnchorPoint::Interpolation::Hermite;
                    } /*else if (dsNode->interpMode() == DsAnchorNode::Cubic) {
                        node.interp = QDspx::AnchorPoint::Interpolation::Cubic;
                    }*/
                    curve->nodes.append(node);
                }
                curves.append(curve);
            }
        }
    };

    auto encodeSingingParam = [&](const DsParam &dsParam, QDspx::ParamInfo &param) {
        encodeCurves(dsParam.original, param.org);
        encodeCurves(dsParam.edited, param.edited);
        encodeCurves(dsParam.envelope, param.envelope);
    };

    auto encodeSingingParams = [&](const DsParams &dsParams, QDspx::SingleParam &params) {
        encodeSingingParam(dsParams.pitch, params.pitch);
        encodeSingingParam(dsParams.energy, params.energy);
        encodeSingingParam(dsParams.tension, params.tension);
        encodeSingingParam(dsParams.breathiness, params.breathiness);
    };

    auto encodePhonemes = [&](const QList<DsPhoneme> &dsPhonemes, QList<QDspx::Phoneme> &phonemes) {
        for (const auto &dsPhoneme : dsPhonemes) {
            QDspx::Phoneme phoneme;
            phoneme.start = dsPhoneme.start;
            phoneme.token = dsPhoneme.name;
            if (dsPhoneme.type == DsPhoneme::DsPhonemeType::Ahead) {
                phoneme.type = QDspx::Phoneme::Type::Ahead;
            } else if (dsPhoneme.type == DsPhoneme::DsPhonemeType::Final) {
                phoneme.type = QDspx::Phoneme::Type::Final;
            } else {
                phoneme.type = QDspx::Phoneme::Type::Normal;
            }
            phonemes.append(phoneme);
        }
    };

    auto encodeNotes = [&](const OverlapableSerialList<DsNote> &dsNotes, QList<QDspx::Note> &notes) {
        for (const auto dsNote : dsNotes) {
            QDspx::Note note;
            note.pos = dsNote->start();
            note.length = dsNote->length();
            note.keyNum = dsNote->keyIndex();
            note.lyric = dsNote->lyric();
            note.pronunciation = dsNote->pronunciation();
            encodePhonemes(dsNote->phonemes().original, note.phonemes.org);
            encodePhonemes(dsNote->phonemes().original, note.phonemes.edited);
            notes.append(note);
        }
    };

    auto encodeClips = [&](const DsTrack *dsTrack, QDspx::Track &track) {
        for (const auto clip : dsTrack->clips()) {
            if (clip->type() == DsClip::Singing) {
                auto singingClip = dynamic_cast<DsSingingClip *>(clip);
                auto singClip = QDspx::SingingClipRef::create();
                singClip->name = clip->name();
                singClip->time.start = clip->start();
                singClip->time.clipStart = clip->clipStart();
                singClip->time.length = clip->length();
                singClip->time.clipLen = clip->clipLen();
                singClip->control.gain = clip->gain();
                singClip->control.mute = clip->mute();
                encodeNotes(singingClip->notes(), singClip->notes);
                encodeSingingParams(singingClip->params, singClip->params);
                track.clips.append(singClip);
            } else if (clip->type() == DsClip::Audio) {
                auto audioClip = dynamic_cast<DsAudioClip *>(clip);
                auto audioClipRef = QDspx::AudioClipRef::create();
                audioClipRef->name = clip->name();
                audioClipRef->time.start = clip->start();
                audioClipRef->time.clipStart = clip->clipStart();
                audioClipRef->time.length = clip->length();
                audioClipRef->time.clipLen = clip->clipLen();
                audioClipRef->control.gain = clip->gain();
                audioClipRef->control.mute = clip->mute();
                audioClipRef->path = audioClip->path();
                track.clips.append(audioClipRef);
            }
        }
    };

    auto encodeTracks = [&](const AppModel *model, QDspx::Model &dspx) {
        for (const auto dsTrack : model->tracks()) {
            QDspx::Track track;
            track.name = dsTrack->name();
            track.control.gain = dsTrack->control().gain();
            track.control.pan = dsTrack->control().pan();
            track.control.mute = dsTrack->control().mute();
            track.control.solo = dsTrack->control().solo();
            encodeClips(dsTrack, track);
            dspx.content.tracks.append(track);
        }
    };

    QDspx::Model dspxModel;
    auto &timeline = dspxModel.content.timeline;
    timeline.tempos.append(QDspx::Tempo(0, model->tempo()));
    timeline.timeSignatures.append(
        QDspx::TimeSignature(0, model->timeSignature().numerator, model->timeSignature().denominator));

    encodeTracks(model, dspxModel);
    auto returnCode = dspxModel.save(path);

    if (returnCode.type != QDspx::ReturnCode::Success) {
        QMessageBox::warning(nullptr, "Warning",
                             QString("Failed to save project file.\r\npath: %1\r\ntype: %2 code: %3")
                                 .arg(path)
                                 .arg(returnCode.type)
                                 .arg(returnCode.code));
        return false;
    }
    return true;
}