#include <QKeyEvent>
#include <QInputDialog>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolTip>
#include "file.h"
#include "player.h"
#include "w_main.h"
#include "w_piano.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

static File clipboard_file;
static vmd_track_t *clipboard = NULL;
static bool clipboard_time_relative;
static bool clipboard_pitch_relative;

const int margin = 50;
const int level_height = 5;
const int scroll_margin = 5;
const int quarter_width = 50;
const int update_freq = 10;

struct PianoPalette : public QPalette
{
	PianoPalette()
	{
		//setColor(Base, Qt::white);
		//setColor(AlternateBase, QColor(0xE0, 0xFF, 0xE8));
		setColor(Highlight, Qt::cyan);
	}
};

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

static int
pitch2level(const vmd_track_t *track, vmd_pitch_t p)
{
	const vmd_notesystem_t *ns = &track->notesystem;
	int octaves = p / ns->size;
	p %= ns->size;
	return octaves * (vmd_notesystem_levels(ns) + 1) + vmd_notesystem_pitch2level(ns, p) + 1;
}

static int
levels(const vmd_track_t *track)
{
	return pitch2level(track, track->notesystem.end_pitch);
}

//TODO: refactor
static vmd_pitch_t
level2pitch(const vmd_track_t *track, int level, bool round_up = false)
{
	if (level < 0)
		return round_up ? 0 : -1;
	if (level >= levels(track))
		return round_up ? VMD_MAX_PITCH : -1;
	const vmd_notesystem_t *ns = &track->notesystem;
	int octaves = level / (vmd_notesystem_levels(ns) + 1);
	level %= (vmd_notesystem_levels(ns) + 1);

	vmd_pitch_t octave_pitch;
	if (level == 0) {
		if (round_up)
			octave_pitch = 0;
		else
			return -1;
	} else {
		while ((octave_pitch = vmd_notesystem_level2pitch(ns, level - 1)) < 0) {
			if (round_up)
				level++;
			else
				return -1;
		}
	}
	return octaves * ns->size + octave_pitch;
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
	int level = pitch2level(note->track, note->pitch);
	arg->min = std::min(arg->min, level);
	arg->max = std::max(arg->max, level);
	return NULL;
}

static int
avg_level(vmd_track_t *track, vmd_time_t beg, vmd_time_t end)
{
	avg_note_arg arg = {
		track,
		levels(track),
		0
	};
	vmd_track_for_range(track, beg, end, avg_note_clb, &arg);
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
	pivot_enabled_(false),
	selection_(NULL),
	player_(_player)
{
	int w = time2x(vmd_file_length(file()));
	int h = margin * 2 + level_height * levels(track());
	setMinimumSize(w, h);
	setPalette(PianoPalette());
	setFocusPolicy(Qt::StrongFocus);

	connect(file_, SIGNAL(acted()), this, SLOT(update()));
	connect(player_, SIGNAL(started()), this, SLOT(playStarted()));
	connect(player_, SIGNAL(finished()), this, SLOT(playStopped()));
	if (playing())
		playStarted();
}

QRect
WPiano::viewport() const {
	QWidget *w = qobject_cast<QWidget *>(parent());
	if (w == NULL)
		w = const_cast<WPiano *>(this);
	QRect geom = w->geometry();
	return QRect(mapFrom(w, geom.topLeft()), mapFrom(w, geom.bottomRight()));
}

vmd_time_t
WPiano::x2time(int x) const
{
	return x * (signed)file()->division / quarter_width;
}

int
WPiano::time2x(vmd_time_t time) const
{
	if (time == VMD_MAX_TIME)
		return width();
	return time * quarter_width / (signed)file()->division;
}

int
WPiano::y2level(int y) const
{
	float flevel = float(height() - margin - y) / level_height;
	return int(flevel + 0.5f);
}

int
WPiano::level2y(int level) const
{
	return height() - (margin + level_height * level);
}

QRect
WPiano::rect2qrect(const Rect &rect) const
{
	int l = time2x(rect.time_beg);
	int r = time2x(rect.time_end);
	int t = rect.level_end > levels(track()) ? 0 : level2y(rect.level_end);
	int b = rect.level_end > levels(track()) ? height() : level2y(rect.level_beg);
	return QRect(l, t, r-l, b-t);
}

WPiano::Rect
WPiano::qrect2rect(const QRect &qrect) const
{
	Rect ret(
		x2time(qrect.left()),
		x2time(qrect.left() + qrect.width()),
		y2level(qrect.top() + qrect.height()),
		y2level(qrect.top())
	);
	return ret;
}

bool
WPiano::timeVisible(vmd_time_t t) const
{
	Rect vp = qrect2rect(viewport());
	return vp.time_beg <= t && t < vp.time_end;
}

vmd_pitch_t
WPiano::cursorPitch() const
{
	return level2pitch(track(), cursor_level_);
}

QRect
WPiano::cursor_qrect() const
{
	if (playing())
		return QRect(time2x(cursor_time_), 0, 1, height());
	else {
		Rect rect(
			cursorTime(),
			cursorEndTime(),
			cursor_level_ - 1,
			cursor_level_ + 1
		);
		return rect2qrect(rect).adjusted(-3, -3, 3, 3);
	}
}

void
WPiano::set_pivot()
{
	if (!pivot_enabled_) {
		setSelection(NULL);
		pivot_time_ = cursor_time_;
		pivot_level_ = cursor_level_;
		pivot_enabled_ = true;
	}
}

void
WPiano::drop_pivot()
{
	if (pivot_enabled_) {
		pivot_enabled_ = false;
		update();
	}
}

void
WPiano::setCursorPos(vmd_time_t time, int level)
{
	if (time != cursor_time_ || level != cursor_level_) {
		update(cursor_qrect());
		cursor_time_ = time;
		cursor_level_ = level;
		update(cursor_qrect());
		if (pivot_enabled_)
			update();
		emit cursorMoved();
	}
}

vmd_note_t *
WPiano::noteAtCursor()
{
	vmd_pitch_t p = cursorPitch();
	if (p < 0)
		return NULL;
	return (vmd_note_t *)vmd_track_range(track(), cursorTime(), cursorEndTime(), p, p + 1);
}

WPiano::Rect
WPiano::selectionRect(bool returnAllIfEmpty) const
{
	Rect ret;

	if (!returnAllIfEmpty && (!pivot_enabled_ || (pivot_time_ == cursor_time_ && pivot_level_ == cursor_level_)))
		return ret;

	if (!pivot_enabled_ || pivot_time_ == cursor_time_) {
		ret.time_beg = 0;
		ret.time_end = VMD_MAX_TIME;
	} else {
		ret.time_beg = std::min(pivot_time_, cursor_time_);
		ret.time_end = std::max(pivot_time_, cursor_time_);
	}

	if (!pivot_enabled_ || pivot_level_ == cursor_level_) {
		ret.level_beg = 0;
		ret.level_end = levels(track()) + 1;
	} else if (pivot_level_ < cursor_level_) {
		ret.level_beg = pivot_level_;
		ret.level_end = cursor_level_;
	} else if (pivot_level_ > cursor_level_) {
		ret.level_beg = cursor_level_ + 1;
		ret.level_end = pivot_level_ + 1;
	}

	return ret;
}

vmd_note_t *
WPiano::selection(bool returnAllIfEmpty) const
{
	if (pivot_enabled_) {
		Rect r = selectionRect(returnAllIfEmpty);
		vmd_pitch_t p_beg = level2pitch(track(), r.level_beg, true);
		vmd_pitch_t p_end = level2pitch(track(), r.level_end, true);
		return vmd_track_range(track(), r.time_beg, r.time_end, p_beg, p_end);
	} else
		return selection_;
}

void
WPiano::setSelection(vmd_note_t *sel)
{
	VMD_BST_FOREACH(vmd_bst_node_t *i, &track_->notes)
		vmd_track_note(i)->mark = 0;
	pivot_enabled_ = false;
	selection_ = sel;
	for (vmd_note_t *i = selection_; i != NULL; i = i->next)
		i->mark = 1;
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
	int mod = ev->modifiers() & (Qt::CTRL | Qt::SHIFT);

#define SHIFT_SELECTS \
	do { \
		if (mod & Qt::SHIFT) { \
			capture_mouse(false); \
			set_pivot(); \
		} else \
			drop_pivot(); \
	} while(0)

	switch (key) {
	case Qt::Key_Escape:
		drop_pivot();
		capture_mouse(false);
		break;
	case Qt::Key_Home:
		SHIFT_SELECTS;
		setCursorTime(0);
		break;
	case Qt::Key_End:
		SHIFT_SELECTS;
		setCursorTime(grid_snap_right(this, vmd_track_length(track_)));
		break;
	case Qt::Key_Left:
		SHIFT_SELECTS;
		setCursorTime(grid_snap_left(this, cursor_time_ - 1));
		break;
	case Qt::Key_Right:
		SHIFT_SELECTS;
		setCursorTime(grid_snap_right(this, cursor_time_ + 1));
		break;
	case Qt::Key_Up:
		SHIFT_SELECTS;
		if (cursor_level_ < levels(track()))
			setCursorLevel(cursor_level_ + 1);
		break;
	case Qt::Key_Down:
		SHIFT_SELECTS;
		if (cursor_level_ > 0)
			setCursorLevel(cursor_level_ - 1);
		break;
	case Qt::Key_Space:
		drop_pivot();
		if (playing())
			player_->stop();
		else
			player_->play(file(), cursor_time_);
		break;
	case Qt::Key_QuoteLeft:
		{
			QString s = QInputDialog::getText(
				this,
				"vomid",
				mod & Qt::CTRL ? QString("Enter grid size:") : QString("Enter cursor size:")
			);
			QStringList sl = s.split("/");
			if (sl.size() != 2)
				break;
			int n = sl[0].toInt();
			int m = sl[1].toInt();
			if (n <= 0 && m <= 0)
				break;
			vmd_time_t size = file()->division * 4 * n / m;
			if (mod & Qt::CTRL)
				grid_size_ = size;
			else
				cursor_size_ = size;
		}
		break;
	case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4:
	case Qt::Key_5: case Qt::Key_6: case Qt::Key_7:
		{
			vmd_time_t size = file()->division * 4;
			for (int i = 0; i < key - Qt::Key_1; i++)
				size /= 2;
			if (mod & Qt::CTRL)
				grid_size_ = size;
			else
				cursor_size_ = size;
		}
		break;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		drop_pivot();
		toggle_note();
		break;
	case Qt::Key_I:
		if (mod == Qt::CTRL) {
			vmd_note_t *note = noteAtCursor();
			if (note == NULL)
				break;
			QPoint pos(time2x(cursor_time_), level2y(cursor_level_));
			QString text = "Note: ";
			text += "channel = "; text += QString::number(note->channel->number);
			QToolTip::hideText();
			QToolTip::showText(mapToGlobal(pos), text);
		}
		break;
	case Qt::Key_C:
		if (mod == Qt::CTRL) {
			if (clipboard == NULL)
				clipboard = vmd_track_create(&clipboard_file, VMD_CHANMASK_ALL);

			Rect s = selectionRect();
			vmd_time_t base_time = s.time_beg;
			int base_pitch = level2pitch(track(), s.level_beg, true);

			clipboard_time_relative = s.time_end != VMD_MAX_TIME;
			clipboard_pitch_relative = s.level_end <= levels(track());
			vmd_track_clear(clipboard);
			for (vmd_note_t *i = selection(); i != NULL; i = i->next)
				vmd_copy_note(i, clipboard, -base_time, -base_pitch);
		}
		break;
	case Qt::Key_V:
		if (mod == Qt::CTRL) {
			vmd_time_t t = clipboard_time_relative ? cursorTime() : 0;
			vmd_pitch_t p = clipboard_pitch_relative ? cursorPitch() : 0;
			if (clipboard == NULL || p < 0)
				break;

			VMD_BST_FOREACH(vmd_bst_node_t *i, &clipboard->notes)
				vmd_copy_note(vmd_track_note(i), track(), t, p);
			file()->commit("Paste Notes");
			drop_pivot();
		}
		break;
	case Qt::Key_Delete:
		for (vmd_note_t *i = selection(); i != NULL; i = i->next)
			vmd_erase_note(i);
		file()->commit("Erase Notes");
		drop_pivot();
	case Qt::Key_T:
		if (mod == Qt::CTRL) {
			int dPitch = QInputDialog::getInt(
				this,
				"vomid",
				QString("Transpose (in semi-tones):")
			);
			if (dPitch == 0)
				break;
			for (vmd_note_t *i = selection(), *next; i != NULL; i = next) {
				next = i->next;
				vmd_copy_note(i, track(), 0, dPitch);
				vmd_erase_note(i);
			}
			file()->commit("Transpose");
		}
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
	QRect v = viewport();
	QPoint p1 = mapToGlobal(v.topLeft());
	QPoint p2 = mapToGlobal(v.bottomRight());
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
	setCursorPos(grid_snap_left(this, x2time(ev->x())), y2level(ev->y()));
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

	QPen pen;
	pen.setWidth(level_height - 1);
	pen.setCapStyle(Qt::FlatCap);
	if (note->mark)
		pen.setColor(Qt::blue);
	painter->setPen(pen);

	int level = pitch2level(piano->track(), note->pitch);
	int y = piano->level2y(level);
	int x1 = piano->time2x(note->on_time);
	int x2 = piano->time2x(note->off_time);
	painter->drawLine(x1, y, x2 - 1, y);
	return NULL;
}

enum {
	LEVEL_NORMAL,
	LEVEL_LINE,
	LEVEL_OCTAVE_LINE
};

static int
level_style(const vmd_track_t *track, int level)
{
	int l = level % (vmd_notesystem_levels(&track->notesystem) + 1);
	if (l == 0)
		return LEVEL_OCTAVE_LINE;
	else if (l % 2 == 0)
		return LEVEL_LINE;
	else
		return LEVEL_NORMAL;
};

void
WPiano::paintEvent(QPaintEvent *ev)
{
	QPainter painter(this);
	QPen pen;
	vmd_time_t beg = x2time(ev->rect().left());
	vmd_time_t end = x2time(ev->rect().right());

	/* background */
	painter.setPen(Qt::NoPen);
	painter.setBrush(palette().base());
	painter.drawRect(0, 0, width(), level2y(levels(track())));
	painter.drawRect(0, level2y(0), width(), height() - level2y(0));
	for (int i = 0; i < (levels(track()) + 1) / 2; i++) {
		painter.setBrush(i % 2 == 0 ? palette().alternateBase() : palette().base());
		painter.drawRect(0, level2y(i*2+2), width(), level2y(i*2) - level2y(i*2+2));
	}

	/* selection */
	if (pivot_enabled_) {
		painter.setBrush(palette().highlight());
		painter.setPen(Qt::NoPen);

		Rect sel = selectionRect();
		sel.level_beg -= 1;
		painter.drawRect(rect2qrect(sel));
	}

	/* vertical grid */
	painter.setPen(QPen());
	vmd_file_measures(file(), beg, end + 1, draw_measure, &painter);

	/* horizontal grid */
	for (int i = 0; i < levels(track()); i++) {
		int style = level_style(track(), i);
		int y = level2y(i);
		if (style != LEVEL_NORMAL) {
			pen.setWidth(style == LEVEL_OCTAVE_LINE ? 2 : 0);
			painter.setPen(pen);
			painter.drawLine(0, y, width(), y);
		}
	}

	/* notes */
	vmd_track_for_range(track(), beg, end, draw_note, &painter);

	/* cursor */
	if (playing()) {
		painter.setPen(QPen());
		int x = time2x(cursor_time_);
		painter.drawLine(x, 0, x, height());
	} else {
		Qt::GlobalColor colors[2] = {
			Qt::black,
			cursorPitch() >= 0 ? Qt::red : Qt::lightGray
		};
		QRect r = cursor_qrect();
		for (int i = 0; i < 2; i++) {
			r.adjust(1, 1, -1, -1);
			painter.setPen(QColor(colors[i]));
			painter.setBrush(Qt::NoBrush);
			painter.drawRect(r);
		}
	}
}

void
WPiano::timerEvent(QTimerEvent *ev)
{
	if (ev->timerId() == update_timer_.timerId()) {
		if (playing() && cursor_time_ != player_->time()) {
			vmd_time_t prev_time = cursor_time_;
			setCursorTime(player_->time());
			if (timeVisible(prev_time))
				look_at_cursor();
		} else if (mouse_captured_)
			clipCursor();
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
		QRect v = viewport();
		QPoint p1 = mapToGlobal(v.topLeft());
		QPoint p2 = mapToGlobal(v.bottomRight());
		RECT rect = {p1.x(), p1.y(), p2.x(), p2.y()};
		ClipCursor(&rect);

		// be sure that cursor doesn't reach the clip boundary
		update_timer_.start(update_freq, this);
#else
		grabMouse();
#endif
		drop_pivot();
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
	QWidget *viewport_widget = qobject_cast<QWidget *>(parent());
	if (viewport_widget == NULL)
		return;
	QScrollArea *scroll = qobject_cast<QScrollArea *>(viewport_widget->parent());
	if (scroll == NULL)
		return;
	QScrollBar *hs = scroll->horizontalScrollBar();
	QScrollBar *vs = scroll->verticalScrollBar();

	bool pl = playing();
	QRect v = viewport();
	int x1, x2, y1, y2;
	x1 = time2x(cursorTime());
	x2 = pl ? x1 + 1 : time2x(cursorEndTime());
	y1 = level2y(cursor_level_ + 1);
	y2 = level2y(cursor_level_ - 1);

	switch (mode) {
	case PAGE:
		if (x1 < v.left())
			hs->setValue(scroll_snap(this, x2 - v.width(), true));
		if (x2 > v.right())
			hs->setValue(scroll_snap(this, x1 - 1));
		if (pl && y1 < v.top())
			vs->setValue(y2 - v.height() + level_height);
		if (pl && y2 > v.bottom())
			vs->setValue(y1 - level_height);
		break;
	case MINSCROLL:
		scroll->ensureVisible(x1, y1, level_height, level_height);
		scroll->ensureVisible(x2, y2, level_height, level_height);
		break;
	case CENTER:
		hs->setValue((x1 + x2 - v.width()) / 2);
		vs->setValue((y1 + y2 - v.height()) / 2);
		break;
	}
}

void
WPiano::adjust_y()
{
	Rect vp = qrect2rect(viewport());
	cursor_level_ = avg_level(track(), vp.time_beg, vp.time_end);
	look_at_cursor(CENTER);
}

void
WPiano::toggle_note()
{
	vmd_pitch_t p = cursorPitch();
	if (p < 0)
		return;
	int erased = vmd_erase_notes(vmd_track_range(track(), cursorTime(), cursorEndTime(), p, p + 1));
	if (erased <= 0) {
		vmd_track_insert(track(), cursorTime(), cursorEndTime(), p);
		file()->commit("Insert Note");
	} else if (erased == 1) {
		file()->commit("Erase Note");
	} else
		file()->commit("Erase Notes");
}

bool
WPiano::playing() const
{
	return player_->file() == file();
}

void
WPiano::playStarted()
{
	if (playing()) {
		capture_mouse(false);
		update_timer_.start(update_freq, this);
	}
}

void
WPiano::playStopped()
{
	update_timer_.stop();
	setCursorTime(grid_snap_left(this, cursor_time_));
	update();
}
