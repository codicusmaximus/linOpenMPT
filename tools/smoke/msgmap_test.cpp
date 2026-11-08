// Checks that MFC message maps written the way upstream writes them route
// commands to the right handlers: exact IDs, ID ranges, update-UI handlers,
// ON_COMMAND_EX fall-through, and lookup through the base class's map.

#include <afxwin.h>

#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr, "FAIL: %s (line %d)\n", #x, __LINE__); failures++; } } while(0)

class TestCmdUI : public CCmdUI
{
public:
	BOOL enabled = -1;
	void Enable(BOOL on) override { enabled = on; }
};

class BaseTarget : public CCmdTarget
{
public:
	int baseCommand = 0;
protected:
	afx_msg void OnBaseCommand() { baseCommand++; }
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(BaseTarget, CCmdTarget)
	ON_COMMAND(100, &BaseTarget::OnBaseCommand)
END_MESSAGE_MAP()

class DerivedTarget : public BaseTarget
{
public:
	int command = 0;
	UINT rangeID = 0;
	UINT exCalls = 0;
protected:
	afx_msg void OnCommand200() { command++; }
	afx_msg void OnRange(UINT id) { rangeID = id; }
	afx_msg void OnUpdate200(CCmdUI *ui) { ui->Enable(FALSE); }
	afx_msg BOOL OnCommandEx(UINT id) { exCalls++; return id == 400; }
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(DerivedTarget, BaseTarget)
	ON_COMMAND(200, &DerivedTarget::OnCommand200)
	ON_COMMAND_RANGE(300, 310, &DerivedTarget::OnRange)
	ON_UPDATE_COMMAND_UI(200, &DerivedTarget::OnUpdate200)
	ON_COMMAND_EX(400, &DerivedTarget::OnCommandEx)
	ON_COMMAND_EX(401, &DerivedTarget::OnCommandEx)
END_MESSAGE_MAP()

int main()
{
	DerivedTarget t;
	CCmdTarget &target = t;

	CHECK(target.OnCmdMsg(200, CN_COMMAND, nullptr, nullptr));
	CHECK(t.command == 1);

	CHECK(target.OnCmdMsg(305, CN_COMMAND, nullptr, nullptr));
	CHECK(t.rangeID == 305);
	CHECK(!target.OnCmdMsg(311, CN_COMMAND, nullptr, nullptr));

	TestCmdUI ui;
	ui.m_nID = 200;
	CHECK(target.OnCmdMsg(200, CN_UPDATE_COMMAND_UI, &ui, nullptr));
	CHECK(ui.enabled == FALSE);

	// Handled by the base class's map.
	CHECK(target.OnCmdMsg(100, CN_COMMAND, nullptr, nullptr));
	CHECK(t.baseCommand == 1);

	// ON_COMMAND_EX: the handler's return value says whether it handled it.
	CHECK(target.OnCmdMsg(400, CN_COMMAND, nullptr, nullptr));
	CHECK(!target.OnCmdMsg(401, CN_COMMAND, nullptr, nullptr));
	CHECK(t.exCalls == 2);

	CHECK(!target.OnCmdMsg(999, CN_COMMAND, nullptr, nullptr));

	if(failures == 0)
		std::puts("message maps: OK");
	return failures == 0 ? 0 : 1;
}
