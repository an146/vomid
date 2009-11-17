/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_PIANO_H
#define W_PIANO_H

#include <QWidget>
#include <QTimer>
#include <vomid.h>

struct vmd_track_t;
class File;
class Player;

class WPiano : public QWidget
{
	Q_OBJECT

public:
	WPiano(File *, vmd_track_t *, vmd_time_t, Player *);

	QRect viewport() const;

	/* time <-> X coord conversion */
	vmd_time_t x2time(int) const;
	int time2x(vmd_time_t) const;
	vmd_time_t l_time() const;
	vmd_time_t r_time() const;

	/* time <-> Y coord conversion */
	int y2level(int) const;
	int level2y(int) const;

	vmd_time_t grid_size() const   { return grid_size_; }
	vmd_time_t cursor_time() const { return cursor_time_; }
	vmd_time_t cursor_size() const { return cursor_size_; }
	vmd_time_t cursor_l() const    { return cursor_time_; }
	vmd_time_t cursor_r() const    { return cursor_time_ + cursor_size_; }
	vmd_pitch_t cursor_pitch() const;
	void setCursorPos(vmd_time_t, int);
	void setCursorTime(vmd_time_t t) { setCursorPos(t, cursor_level_); }
	void setCursorLevel(int l) { setCursorPos(cursor_time_, l); }
	vmd_note_t *noteAtCursor();

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

	enum LookMode {
		PAGE,
		MINSCROLL,
		CENTER
	};
	void look_at_cursor(LookMode mode = PAGE);
	void capture_mouse(bool capture = true);

protected slots:
	void playStarted();
	void playUpdate();
	void playStopped();
	void clipCursor();

private:
	QTimer update_timer_;

	File *file_;
	vmd_track_t *track_;
	vmd_time_t grid_size_;
	bool mouse_captured_;

	vmd_time_t cursor_time_, cursor_size_;
	int cursor_level_;

	Player *player_;
};

#endif /* W_PIANO_H */
