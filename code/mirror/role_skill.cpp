#include "role_skill.h"

extern QMap<skillID, Info_skill> g_skillList;

role_skill::role_skill(QWidget *parent, RoleVoc voc, MapRoleSkill *skill_study)
	: QDialog(parent)
	,m_voc(voc), m_skill_study(skill_study)
{
	ui.setupUi(this);
	InitUI(voc);


	for (qint32 i = 0; i < skillBtn.size(); i++)
	{
		connect(skillBtn[i], &QPushButton::clicked, [=]() {
			process_btn_Tree(i);});
	}
	connect(ui.checkBox_use, SIGNAL(stateChanged(int)), this, SLOT(on_checkBox_used_stateChanged(int)));


	process_btn_Tree(0);
}

role_skill::~role_skill()
{

}

void role_skill::closeEvent(QCloseEvent *event)
{
	on_btn_close_clicked();
}

void role_skill::on_btn_ok_clicked(void)
{
	bool bAtLeastOne = false;
	skillID id;
	for (qint32 i = 0; i < skillCheck.size(); i++)
	{
		id = skillCheck[i]->whatsThis().toUInt();
		if (m_skill_study->contains(id))
		{
			roleSkill2 sk2 = m_skill_study->value(id);
			sk2.Used = skillCheck[i]->isChecked();

			m_skill_study->insert(id, sk2);
		}	
	}

	for (auto iter = m_skill_study->constBegin(); iter != m_skill_study->constEnd(); iter++)
	{
		if (iter->Used)
		{
			bAtLeastOne = true;
			break;
		}
	}

	if (!bAtLeastOne)
	{
		roleSkill2 sk2 = m_skill_study->first();
		sk2.Used = true;
		m_skill_study->insert(sk2.id, sk2);
	}
	done(QDialog::Accepted);
}
void role_skill::on_btn_close_clicked(void)
{
	done(QDialog::Rejected);
}

inline const Info_skill *role_skill::FindSkill(skillID id)
{
	foreach(const Info_skill &info, g_skillList)
	{
		if (info.ID == id)
		{
			return &info;
		}
	}
	return nullptr;
}

bool role_skill::InitUI(RoleVoc voc)
{
	//先去掉基本技能。
	roleSkill2 sk2 = m_skill_study->first();
	sk2.Used = false;
	m_skill_study->insert(sk2.id, sk2);

	bool bRes = true;

	//不显示第一个技能。
	auto iter = g_skillList.constBegin();
	iter++;
	for (; iter != g_skillList.constEnd(); iter++)
	{
		if (voc == iter->type)
		{
			QPushButton *btn = new QPushButton(this);
			btn->setIconSize(QSize(34, 34));
			btn->setIcon(iter->icon);
			btn->setWhatsThis(QString::number(iter->ID));
			skillBtn.append(btn);

			QCheckBox *check = new QCheckBox(this);
			check->setWhatsThis(QString::number(iter->ID));
			skillCheck.append(check);

			check->setEnabled(m_skill_study->contains(iter->ID));

			if (m_skill_study->contains(iter->ID))
			{
				check->setChecked(m_skill_study->value(iter->ID).Used);
			}
		}
	}

	QSize BtnSize(42, 42);
	QSize CheckSize(13, 13);
	if (voc == Voc_Warrior)
	{
		bRes = InitSkillTree_Warrior(BtnSize, CheckSize);
	} else if (voc == Voc_Magic)
	{
		bRes = InitSkillTree_Magic(BtnSize, CheckSize);
	} else if (voc == Voc_Taoist)
	{
		bRes = InitSkillTree_Taoist(BtnSize, CheckSize);
	}

	return bRes;
}

bool role_skill::InitSkillTree_Warrior(const QSize& btnSize, const QSize& CheckSize)
{
	if (skillBtn.size() != 9)
	{
		return false;
	}

	//连接线的位置及大小。
	QRect rtLine_V[9] = { QRect(83, 50, 36, 21), QRect(83, 110, 36, 25), QRect(46, 134, 36, 40), QRect(46, 210, 36, 31), QRect(46, 280, 36, 31),
		QRect(46, 350, 36, 31), QRect(125, 135, 36, 60), QRect(125, 230, 36, 31), QRect(125, 300, 36, 31) };
	QRect rtLine_H[1] = { QRect(60, 120, 87, 36) };

	CreateLine_H(rtLine_H, 1);
	CreateLine_V(rtLine_V, 9);

	//技能格式的位置。
	QPoint point[9] = { QPoint(80, 10), QPoint(80, 70), QPoint(40, 170), QPoint(120, 190), QPoint(40, 240),
		QPoint(120, 260), QPoint(120, 330), QPoint(40, 310), QPoint(40, 380) };

	QPoint ptOffset = QPoint(btnSize.width(), btnSize.height()) - QPoint(0, CheckSize.height()) - QPoint(2, 1);
	CreateSkillBtn(btnSize, CheckSize, point, ptOffset);
	return true;
}
bool role_skill::InitSkillTree_Magic(const QSize& btnSize, const QSize& CheckSize)
{
	if (skillBtn.size() != 11)
	{
		return false;
	}

	//连接线的位置及大小。
	QRect rtLine_V[8] = { QRect(53, 51, 36, 25), QRect(53, 114, 36, 23), QRect(13, 134, 36, 38), QRect(85, 135, 36, 38), QRect(85, 210, 36, 150),
		QRect(154, 130, 36, 61), QRect(154, 230, 36, 31), QRect(154, 300, 36, 31) };
	QRect rtLine_H[1] = { QRect(28, 120, 79, 36) };

	CreateLine_H(rtLine_H, 1);
	CreateLine_V(rtLine_V, 8);

	QPoint point[11] = { QPoint(50, 10), QPoint(50, 74), QPoint(150, 90), QPoint(150, 190), QPoint(10, 170),
		QPoint(80, 170), QPoint(150, 260), QPoint(10, 230), QPoint(10, 290), QPoint(150, 330), QPoint(80, 360) };

	QPoint ptOffset = QPoint(btnSize.width(), btnSize.height()) - QPoint(0, CheckSize.height()) - QPoint(2, 1);
	CreateSkillBtn(btnSize, CheckSize, point, ptOffset);
	
	return true;
}
bool role_skill::InitSkillTree_Taoist(const QSize& btnSize, const QSize& CheckSize)
{
	if (skillBtn.size() != 11)
	{
		return false;
	}

	//连接线的位置及大小。
	QRect rtLine_V[6] = { QRect(15, 90, 36, 51), QRect(15, 260, 36, 91), QRect(84, 50, 36, 41), QRect(84, 130, 36, 121), QRect(155, 110, 36, 21),
		QRect(155, 230, 36, 31) };

	CreateLine_V(rtLine_V, 9);

	QPoint point[11] = { QPoint(10, 50), QPoint(80, 10), QPoint(150, 70), QPoint(80, 90), QPoint(150, 190),
		QPoint(150, 260), QPoint(150, 130), QPoint(10, 220), QPoint(10, 140), QPoint(10, 350), QPoint(80, 250) };

	QPoint ptOffset = QPoint(btnSize.width(), btnSize.height()) - QPoint(0, CheckSize.height()) - QPoint(2, 1);
	CreateSkillBtn(btnSize, CheckSize, point, ptOffset);

	return true;
}

bool role_skill::CreateSkillBtn(const QSize& btnSize, const QSize& CheckSize, const QPoint *point, const QPoint &ptOffset)
{
	for (int i = 0; i < skillBtn.size(); i++)
	{
		skillBtn[i]->setGeometry(QRect(point[i], btnSize));
		skillCheck[i]->setGeometry(QRect(point[i] + ptOffset, CheckSize));
	}
	return true;
}

bool role_skill::CreateLine_V(const QRect *rtLine, qint32 nCount)
{
	for (qint32 i = 0; i < nCount; i++)
	{
		QLabel *lbl = new QLabel(this);
		lbl->setGeometry(rtLine[i]);
		lbl->setPixmap(QPixmap(":/mirror/Resources/Line_V.png"));
		lbl->setScaledContents(true);
	}
	return true;
}

bool role_skill::CreateLine_H(const QRect *rtLine, qint32 nCount)
{
	for (qint32 i = 0; i < nCount; i++)
	{
		QLabel *lbl = new QLabel(this);
		lbl->setGeometry(rtLine[i]);
		lbl->setPixmap(QPixmap(":/mirror/Resources/Line_H.png"));
		lbl->setScaledContents(true);
	}
	return true;
}

void role_skill::process_btn_Tree(quint32 nIndex)
{
	skillID id = skillBtn[nIndex]->whatsThis().toUInt();
	const Info_skill *skill = FindSkill(id);
	if (skill != nullptr)
	{
		ui.btn_skill->setIcon(skill->icon);

		ui.lbl_name->setText(skill->name);
		ui.lbl_name->setWhatsThis(QString::number(nIndex));
		ui.lbl_lv->setText(QString("%1 / %2").arg(m_skill_study->value(id).level).arg(skill->level));
		ui.checkBox_use->setChecked(skillCheck[nIndex]->isChecked());
		ui.checkBox_use->setEnabled(m_skill_study->value(id).level > 0);


		QString strTmp;
		if (skill->times !=0) {
			strTmp = QStringLiteral("主动攻击技能");
		} else if (skill->buff > 100) 	{
			strTmp = QStringLiteral("减益技能");
		} else if (skill->buff > 0) 	{
			strTmp = QStringLiteral("增益技能");
		}  else 	{
			strTmp = QStringLiteral("未知类型");
		}
		ui.edit_cur->setText(QStringLiteral("<font color = green>技能类型：%1 </font>").arg(strTmp));
		ui.edit_cur->append(QStringLiteral("<font color = green>技能消耗：%1 </font>").arg(skill->spell_basic + skill->spell_add * m_skill_study->value(id).level));
		ui.edit_cur->append(QStringLiteral("<font color = green>冷却时间：%1 </font>").arg(skill->cd));
		ui.edit_cur->append(QStringLiteral("<font color = green>技能效果:</font>"));
		ui.edit_cur->append(QStringLiteral("<font color = black>   %1 </font>").arg(skill->descr));
	}
}

void role_skill::on_checkBox_used_stateChanged(int state)
{
	qint32 nIndex = ui.lbl_name->whatsThis().toUInt();

	skillCheck[nIndex]->setCheckState(static_cast<Qt::CheckState>(state));
}