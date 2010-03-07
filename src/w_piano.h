/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_PIANO_H
#define W_PIANO_H

#include <QWidget>
#include <QBasicTimer>
#include <vomid.h>

struct vmd_track_t;
class File;
class Player;

class WPiano : public QWidget
{
	Q_OBJECT

public:
	struct Rect
	{
		vmd_time_t time_beg, time_end;
		int level_beg, level_end;

		Rect() : time_beg(0), time_end(0), level_beg(0), level_end(0) { }

		Rect(vmd_time_t _time_beg, vmd_time_t _time_end, int _level_beg, int _level_end)
			: time_beg(_time_beg),
			time_end(_time_end),
			level_beg(_level_beg),
			level_end(_level_end)
		{
		}
	};

	WPiano(File *, vmd_track_t *, vmd_time_t, Player *);

	QRect viewport() const;

	/* coord conversion */
	vmd_time_t x2time(int) const;
	int time2x(vmd_time_t) const;
	int y2level(int) const;
	int level2y(int) const;
	QRect rect2qrect(const Rect &) const;
	Rect qrect2rect(const QRect &) const;

	bool timeVisible(vmd_time_t) const;

	vmd_time_t grid_size() const   { return grid_size_; }
	vmd_time_t cursorTime() const { return cursor_time_; }
	vmd_time_t cursorEndTime() const { return cursor_time_ + cursor_size_; }
	vmd_time_t cursorSize() const { return cursor_size_; }
	vmd_pitch_t cursorPitch() const;
	void setCursorPos(vmd_time_t, int);
	void setCursorTime(vmd_time_t t) { setCursorPos(t, cursor_level_); }
	void setCursorLevel(int l) { setCursorPos(cursor_time_, l); }
	vmd_note_t *noteAtCursor();
	Rect selectionRect(bool returnAllIfEmpty = false) const;
	vmd_note_t *selection(bool returnAllIfEmpty = false) const;

	vmd_track_t *track() const { return track_; }
	File *file() const { return file_; }
	Player *player() const { return player_; }

	void adjust_y();
	void toggle_note();
	bool playing() const;

signals:
	void cursorMoved();

protected:
	void focusOutEvent(QFocusEvent *);
	void keyPressEvent(QKeyEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void mousePressEvent(QMouseEvent *);
	void paintEvent(QPaintEvent *);
	void timerEvent(QTimerEvent *);

	enum LookMode {
		PAGE,
		MINSCROLL,
		CENTER
	};
	void look_at_cursor(LookMode mode = PAGE);
	void capture_mouse(bool capture = true);
	QRect cursor_qrect() const;
	void set_pivot();
	void drop_pivot();

protected slots:
	void playStarted();
	void playStopped();
	void clipCursor();

private:
	QBasicTimer update_timer_;

	File *file_;
	vmd_track_t *track_;
	vmd_time_t grid_size_;
	bool mouse_captured_;

	vmd_time_t cursor_time_, cursor_size_;
	int cursor_level_;

	bool selection_enabled_;
	vmd_time_t pivot_time_;
	int pivot_level_;

	Player *player_;
};

#endif /* W_PIANO_H */
