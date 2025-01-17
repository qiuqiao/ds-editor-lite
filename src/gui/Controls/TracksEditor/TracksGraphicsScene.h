//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_TRACKSGRAPHICSSCENE_H
#define DATASET_TOOLS_TRACKSGRAPHICSSCENE_H

#include "../Base/CommonGraphicsScene.h"
#include "Controls/Base/TimeGraphicsScene.h"

class TracksGraphicsScene final : public TimeGraphicsScene {
public:
    explicit TracksGraphicsScene();
    int trackIndexAt(double sceneY);
    int tickAt(double sceneX);

public slots:
    // void onProjectLengthChanged(int length); // tick
    void onViewResized(QSize size);
    void onTrackCountChanged(int count);

private:
    void updateSceneRect() override;

    int m_trackCount = 0;
    QSize m_graphicsViewSize = QSize(0, 0);
};


#endif // DATASET_TOOLS_TRACKSGRAPHICSSCENE_H
