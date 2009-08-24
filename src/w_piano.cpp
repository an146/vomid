#include <QKeyEvent>
#include <QInputDialog>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QScrollBar>
#include "file.h"
#include "player.h"
#include "w_main.h"
#include "w_piano.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

const int margin = 50;
const int level_height = 5;
const int scroll_margin = 5;
const int quarter_width = 50;

static vmd_time_t
grid_snap_left(WPiano *piano, vmd_time_t time)
{
	if (time < 0)
		return 0;
	vmd_measure_t measure;
	vmd_file_measure_at(piano->file(), time, &measure);

	vmd_time_t ret = measure.beg;
	while (ret + piano->grid_size() <= time)
		ret += piano->grid_size();
	return ret;
}

static vmd_time_t
grid_snap_right(WPiano *piano, vmd_time_t time)
{
	if (time < 0)
		return 0;
	vmd_measure_t measure;
	vmd_file_measure_at(piano->file(), time, &measure);

	vmd_time_t ret = measure.beg;
	while (ret <= time)
		ret += piano->grid_size();
	return std::min(ret, measure.end);
}

struct avg_note_arg {
	vmd_track_t *track;
	int min;
	int max;
};

static void *
avg_note_clb(vmd_note_t *note, void *_arg)
{
	avg_note_arg *arg = (avg_note_arg *)_arg;
	int level = arg->track->rendersystem->pitch2level(note->pitch);
	arg->min = std::min(arg->min, level);
	arg->max = std::max(arg->max, level);
	return NULL;
}

static int
avg_level(vmd_track_t *track, vmd_time_t beg, vmd_time_t end)
{
	avg_note_arg arg = {
		track,
		track->rendersystem->levels,
		0
	};
	vmd_track_range(track, beg, end, avg_note_clb, &arg);
	return (arg.min + arg.max) / 2;
}

WPiano::WPiano(File *_file, vmd_track_t *_track, vmd_time_t time, Player *_player)
	:file_(_file),
	track_(_track),
	grid_size_(file()->division),
	mouse_captured_(false),
	cursor_time_(grid_snap_left(this, time)),
	cursor_size_(file()->division),
	cursor_level_(0),
	player_(_player)
{
	int w = time2x(vmd_file_length(file()));
	int h = margin * 2 + level_height * track()->rendersystem->levels;
	setMinimumSize(w, h);
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);
	setFocusPolicy(Qt::StrongFocus);
	update_timer_.setInterval(10);

	connect(file_, SIGNAL(acted()), this, SLOT(update()));
	connect(player_, SIGNAL(started()), this, SLOT(playStarted()));
	connect(&update_timer_, SIGNAL(timeout()), this, SLOT(playUpdate()));
	connect(&update_timer_, SIGNAL(timeout()), this, SLOT(clipCursor()));
	connect(player_, SIGNAL(finished()), this, SLOT(playStopped()));
	if (playing())
		playStarted();
}

int
WPiano::time2x(vmd_time_t time) const
{
	return time * quarter_width / (signed)file()->division;
}

vmd_time_t
WPiano::x2time(int x) const
{
	return x * (signed)file()->division / quarter_width;
}

int
WPiano::level2y(int level) const
{
	return height() - (margin + level_height * level);
}

vmd_pitch_t
WPiano::cursor_pitch() const
{
	return vmd_rendersystem_level2pitch(track()->rendersystem, cursor_level_);
}

bool
WPiano::playing() const
{
	return player_->file() == file();
}

vmd_time_t
WPiano::x_time() const
{
	QWidget *viewport = qobject_cast<QWidget *>(parent());
	if (viewport == NULL)
		return 0;
	QPoint p = mapFrom(viewport, viewport->geometry().topLeft());
	return x2time(p.x());
}

vmd_time_t
WPiano::r_time() const
{
	QWidget *viewport = qobject_cast<QWidget *>(parent());
	if (viewport == NULL)
		return width();
	QPoint p = mapFrom(viewport, viewport->geometry().bottomRight());
	return x2time(p.x());
}

void
WPiano::focusOutEvent(QFocusEvent *)
{
	capture_mouse(false);
}

void
WPiano::keyPressEvent(QKeyEvent *ev)
{
	int key = ev->key();
	key |= ev->modifiers() & (Qt::CTRL | Qt::SHIFT);

	switch (key) {
	case Qt::Key_Escape:
		capture_mouse(false);
		break;
	case Qt::Key_Left:
		cursor_time_ = grid_snap_left(this, cursor_time_ - 1);
		break;
	case Qt::Key_Right:
		cursor_time_ = grid_snap_right(this, cursor_time_ + 1);
		break;
	case Qt::Key_Up:
		if (cursor_level_ < track()->rendersystem->levels)
			cursor_level_++;
		break;
	case Qt::Key_Down:
		if (cursor_level_ > 0)
			cursor_level_--;
		break;
	case Qt::Key_Space:
		if (playing())
			player_->stop();
		else
			player_->play(file(), cursor_time_);
		break;
	case Qt::Key_N:
	case Qt::Key_M:
		{
			QString s = QInputDialog::getText(
				this,
				"vomid",
				key == Qt::Key_N ? QString("Enter cursor size:") : QString("Enter grid size:")
			);
			QStringList sl = s.split("/");
			if (sl.size() != 2)
				break;
			int n = sl[0].toInt();
			int m = sl[1].toInt();
			if (n <= 0 && m <= 0)
				break;
			vmd_time_t size = file()->division * 4 * n / m;
			if (key == Qt::Key_N)
				cursor_size_ = size;
			else
				grid_size_ = size;
		}
		break;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		toggle_note();
		break;
	default:
		return QWidget::keyPressEvent(ev);
	}
	look_at_cursor();
	update();
}

static int
clipped(int x, int s, int e)
{
	const int margin = 1;
	if (x < s + margin)
		return s + margin;
	else if (x >= e - margin)
		return e - margin - 1;
	else
		return x;
}

void
WPiano::clipCursor()
{
	if (!mouse_captured_)
		return;
	QWidget *viewport = qobject_cast<QWidget *>(parent());
	if (viewport == NULL)
		return;
	QPoint p1 = viewport->mapToGlobal(viewport->geometry().topLeft());
	QPoint p2 = viewport->mapToGlobal(viewport->geometry().bottomRight());
	int x = clipped(QCursor::pos().x(), p1.x(), p2.x());
	int y = clipped(QCursor::pos().y(), p1.y(), p2.y());
	QCursor::setPos(QPoint(x, y));
}

void
WPiano::mouseMoveEvent(QMouseEvent *ev)
{
	if (!mouse_captured_)
		return;

	clipCursor();

	cursor_time_ = grid_snap_left(this, x2time(ev->x()));
	float flevel = float(height() - margin - ev->y()) / level_height;
	cursor_level_ = int(flevel + 0.5f);
	update();

	look_at_cursor(MINSCROLL);
}

void
WPiano::mousePressEvent(QMouseEvent *ev)
{
	switch (ev->button()) {
	case Qt::LeftButton:
		if (!mouse_captured_) {
			capture_mouse();
			mouseMoveEvent(ev);
		} else
			toggle_note();
		break;
	default:
		;
	}
}

static void
draw_measure(const vmd_measure_t *measure, void *_painter)
{
	QPainter *painter = static_cast<QPainter *>(_painter);
	WPiano *piano = static_cast<WPiano *>(painter->device());
	vmd_time_t cell_size = piano->grid_size() > 0 ? piano->grid_size() : measure->part_size;

	painter->setPen(piano->palette().windowText().color());
	for (vmd_time_t t = measure->beg; t < measure->end; t += cell_size) {
		int x = piano->time2x(t);
		painter->drawLine(x, 0, x, piano->height());
		painter->setPen(piano->palette().mid().color());
	}
}

static void *
draw_note(vmd_note_t *note, void *_painter)
{
	QPainter *painter = static_cast<QPainter *>(_painter);
	WPiano *piano = static_cast<WPiano *>(painter->device());

	int level = piano->track()->rendersystem->pitch2level(note->pitch);
	int y = piano->level2y(level);
	int x1 = piano->time2x(note->on_time);
	int x2 = piano->time2x(note->off_time);
	painter->drawLine(x1, y, x2 - 1, y);
	return NULL;
}

void
WPiano::paintEvent(QPaintEvent *ev)
{
	QPainter painter(this);
	QPen pen;
	vmd_time_t beg = x2time(ev->rect().left());
	vmd_time_t end = x2time(ev->rect().right());

	/* vertical grid */
	vmd_file_measures(file(), beg, end + 1, draw_measure, &painter);

	/* horizontal grid */
	for (int i = 0; i < track()->rendersystem->levels; i++) {
		int style = track()->rendersystem->level_style(i);
		int y = level2y(i);
		if (style != VMD_LEVEL_NORMAL) {
			pen.setWidth(style == VMD_LEVEL_OCTAVE_LINE ? 2 : 0);
			painter.setPen(pen);
			painter.drawLine(0, y, width(), y);
		}
	}

	/* notes */
	pen.setWidth(level_height - 1);
	pen.setCapStyle(Qt::FlatCap);
	painter.setPen(pen);
	vmd_track_range(track(), beg, end, draw_note, &painter);

	/* cursor */
	if (playing()) {
		painter.setPen(QPen());
		int x = time2x(player_->time());
		painter.drawLine(x, 0, x, height());
	} else {
		Qt::GlobalColor colors[3] = {
			Qt::white, // not used
			cursor_pitch() >= 0 ? Qt::red : Qt::lightGray,
			Qt::black
		};
		for (int i = 1; i < 3; i++) {
			int y1 = level2y(cursor_level_ + 1) - i;
			int y2 = level2y(cursor_level_ - 1) + i;
			int x1 = time2x(cursor_time_) - i;
			int x2 = time2x(cursor_time_ + cursor_size_) + i;
			painter.setPen(QColor(colors[i]));
			painter.drawRect(x1, y1, x2 - x1, y2 - y1);
		}
	}
}

void
WPiano::capture_mouse(bool capture)
{
	if (capture == mouse_captured_)
		return;
	if (capture && playing())
		return;

	if (capture) {
#ifdef Q_WS_WIN
		QWidget *viewport = qobject_cast<QWidget *>(parent());
		QPoint p1 = viewport->mapToGlobal(viewport->geometry().topLeft());
		QPoint p2 = viewport->mapToGlobal(viewport->geometry().bottomRight());
		RECT rect = {p1.x(), p1.y(), p2.x(), p2.y()};
		ClipCursor(&rect);

		// be sure that cursor doesn't reach the clip boundary
		update_timer_.start();
#else
		grabMouse();
#endif
		setCursor(Qt::BlankCursor);
	} else {
#ifdef Q_WS_WIN
		ClipCursor(NULL);
		update_timer_.stop();
#else
		releaseMouse();
#endif
		setCursor(Qt::ArrowCursor);
	}
	mouse_captured_ = capture;
	setMouseTracking(mouse_captured_);
}

static int
scroll_snap(WPiano *piano, int x, bool right = false)
{
	if (x < 0)
		return 0;
	vmd_measure_t measure;
	vmd_file_measure_at(piano->file(), piano->x2time(x), &measure);
	return piano->time2x(right ? measure.end : measure.beg);
}

void
WPiano::look_at_cursor(LookMode mode)
{
	QWidget *viewport = qobject_cast<QWidget *>(parent());
	if (viewport == NULL)
		return;
	QScrollArea *scroll = qobject_cast<QScrollArea *>(viewport->parent());
	if (scroll == NULL)
		return;
	QScrollBar *hs = scroll->horizontalScrollBar();
	QScrollBar *vs = scroll->verticalScrollBar();

	QPoint p1 = mapFrom(viewport, viewport->geometry().topLeft());
	QPoint p2 = mapFrom(viewport, viewport->geometry().bottomRight());
	int x1, x2, y1, y2;
	x1 = time2x(cursor_time());
	if (playing()) {
		x2 = x1 + 1;
		y1 = y2 = (p1.y() + p2.y()) / 2;
	} else {
		x2 = time2x(cursor_time_ + cursor_size());
		y1 = level2y(cursor_level_ + 1);
		y2 = level2y(cursor_level_ - 1);
	}

	switch (mode) {
	case PAGE:
		if (x1 < p1.x())
			hs->setValue(scroll_snap(this, x2 - viewport->width(), true));
		if (x2 > p2.x())
			hs->setValue(scroll_snap(this, x1 - 1));
		if (y1 < p1.y())
			vs->setValue(y2 - viewport->height() + level_height);
		if (y2 > p2.y())
			vs->setValue(y1 - level_height);
		break;
	case MINSCROLL:
		scroll->ensureVisible(x1, y1, level_height, level_height);
		scroll->ensureVisible(x2, y2, level_height, level_height);
		break;
	case CENTER:
		hs->setValue((x1 + x2 - viewport->width()) / 2);
		vs->setValue((y1 + y2 - viewport->height()) / 2);
		break;
	}
}

void
WPiano::adjust_y()
{
	cursor_level_ = avg_level(track(), x_time(), r_time());
	look_at_cursor(CENTER);
}

void
WPiano::toggle_note()
{
	vmd_pitch_t p = cursor_pitch();
	if (p < 0)
		return;
	int erased = vmd_track_erase_range(track(), cursor_x(), cursor_r(), p, p + 1);
	if (erased <= 0) {
		vmd_track_insert(track(), cursor_time(), cursor_time() + cursor_size(), p);
		file()->commit("Insert Note");
	} else if (erased == 1) {
		file()->commit("Erase Note");
	} else
		file()->commit("Erase Notes");
}

void
WPiano::playStarted()
{
	if (playing()) {
		capture_mouse(false);
		update_timer_.start();
		playUpdate();
	}
}

void
WPiano::playUpdate()
{
	if (!playing() || cursor_time_ == player_->time())
		return;
	vmd_time_t prev_time = cursor_time_;
	cursor_time_ = player_->time();
	if (x_time() <= prev_time && prev_time <= r_time())
		look_at_cursor();
	update();
}

void
WPiano::playStopped()
{
	update_timer_.stop();
	cursor_time_ = grid_snap_left(this, cursor_time_);
	update();
}
