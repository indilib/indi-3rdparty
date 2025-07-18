#pragma once

#include <QScrollBar>

class BallScrollBar : public QScrollBar
{
  Q_OBJECT
  Q_PROPERTY(bool ballVisible READ ballVisible WRITE setBallVisible)
  Q_PROPERTY(QColor ballColor READ ballColor WRITE setBallColor)
  Q_PROPERTY(int ballValue READ ballValue WRITE setBallValue)

public:
  using QScrollBar::QScrollBar;

public Q_SLOTS:
  void setBallVisible(bool visible);
  bool ballVisible()          { return ball_visible; }

  void setBallColor(QColor const & color);
  QColor const & ballColor()  { return ball_color; }

  void setBallValue(int value);
  int ballValue()             { return ball_value; }

Q_SIGNALS:
  void scrollingFinished();

protected:
  void mouseReleaseEvent(QMouseEvent * event) override;
  void keyReleaseEvent(QKeyEvent * event) override;

#ifndef QT_NO_WHEELEVENT
    void wheelEvent(QWheelEvent * event) override;
#endif

#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(QContextMenuEvent * event) override;
#endif

  void paintEvent(QPaintEvent * event) override;

private:
  bool ball_visible = false;
  QColor ball_color = Qt::black;
  int ball_value = 0;
};