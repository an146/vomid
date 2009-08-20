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

	int level2y(int) const;
	int time2x(vmd_time_t) const;
	vmd_time_t x2time(int) const;
	vmd_pitch_t cursor_pitch() const;
	bool playing() const;
	vmd_time_t x_time() const;
	vmd_time_t r_time() const;

	int grid_size() const { return grid_size_; }
	vmd_time_t cursor_time() const { return cursor_time_; }
	vmd_time_t cursor_size() const { return cursor_size_; }
	vmd_time_t cursor_x() const    { return cursor_time_; }
	vmd_time_t cursor_r() const    { return cursor_time_ + cursor_size_; }

	vmd_track_t *track() { return track_; }
	const vmd_track_t *track() const { return track_; }
	File *file() { return file_; }
	const File *file() const { return file_; }
	Player *player() { return player_; }

	void adjust_y();

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

private:
	QTimer update_timer_;

	File *file_;
	vmd_track_t *track_;
	int grid_size_;
	bool mouse_captured_;

	vmd_time_t cursor_time_, cursor_size_;
	int cursor_level_;

	Player *player_;
};

#endif /* W_PIANO_H */
