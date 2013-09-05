﻿// 通用技能
#include <LCUI_Build.h>
#include LC_LCUI_H
#include LC_WIDGET_H
#include "../game.h"
#include "game_skill.h"

#define LIFT_HEIGHT	56

#define SHORT_REST_TIMEOUT	2000
#define LONG_REST_TIMEOUT	3000
#define BE_THROW_REST_TIMEOUT	500
#define BE_LIFT_REST_TIMEOUT	4500

#define XSPEED_RUN	60
#define XSPEED_WALK	15
#define YSPEED_WALK	8
#define XACC_STOPRUN	20
#define XACC_DASH	10
#define ZSPEED_JUMP	60
#define ZACC_JUMP	22

#define XSPEED_S_HIT_FLY	55
#define ZACC_S_HIT_FLY		7
#define ZSPEED_S_HIT_FLY	15

#define XACC_ROLL	15

#define XSPEED_WEAK_WALK 20

#define XSPEED_X_HIT_FLY	70
#define ZACC_XB_HIT_FLY		15
#define ZSPEED_XB_HIT_FLY	25
#define ZACC_XF_HIT_FLY		50
#define ZSPEED_XF_HIT_FLY	100

#define XSPEED_X_HIT_FLY2	20
#define ZACC_XF_HIT_FLY2	10
#define ZSPEED_XF_HIT_FLY2	15

#define XSPEED_HIT_FLY	20
#define ZSPEED_HIT_FLY	100
#define ZACC_HIT_FLY	50

#define ROLL_TIMEOUT	200

#define XSPEED_THROWUP_FLY	60
#define XSPEED_THROWDOWN_FLY	70
#define ZSPEED_THROWUP_FLY	60
#define ZSPEED_THROWDOWN_FLY	40
#define ZACC_THROWUP_FLY	30
#define ZACC_THROWDOWN_FLY	40

static LCUI_BOOL GamePlayerStateCanNormalAttack( GamePlayer *player )
{
	switch(player->state) {
	case STATE_READY:
	case STATE_WALK:
	case STATE_STANCE:
	case STATE_BE_LIFT_STANCE:
		return TRUE;
	default: 
		break;
	}
	return FALSE;
}

static LCUI_BOOL GamePlayerStateCanJumpAttack( GamePlayer *player )
{
	switch(player->state) {
	case STATE_JUMP:
	case STATE_JSQUAT:
		return TRUE;
	default: 
		break;
	}
	return FALSE;
}

static LCUI_BOOL GamePlayerStateCanSprintJumpAttack( GamePlayer *player )
{
	switch(player->state) {
	case STATE_SJUMP:
	case STATE_SSQUAT:
		return TRUE;
	default: 
		break;
	}
	return FALSE;
}

static LCUI_BOOL GamePlayerStateCanThrow( GamePlayer *player )
{
	switch(player->state) {
	case STATE_LIFT_JUMP:
	case STATE_LIFT_FALL:
	case STATE_LIFT_RUN:
	case STATE_LIFT_STANCE:
	case STATE_LIFT_WALK:
		return TRUE;
	default:break;
	}
	return FALSE;
}

/** 处于被举起的状态下，在攻击结束时  */
static void GamePlayer_AtBeLiftAttackDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	player->attack_type = ATTACK_TYPE_NONE;
	GamePlayer_ChangeState( player, STATE_BE_LIFT_STANCE );
}

/** 在攻击结束时  */
static void GamePlayer_AtAttackDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GameObject_AtXSpeedToZero( widget, 0, NULL );
	player->attack_type = ATTACK_TYPE_NONE;
	GamePlayer_ResetAttackControl( player );
	GamePlayer_UnlockAction( player );
	GamePlayer_UnlockMotion( player );
	GamePlayer_SetReady( player );
}

/** 检测是否能够使用终结一击 */
static LCUI_BOOL CommonSkill_CanUseFinalBlow( GamePlayer *player )
{
	LCUI_Widget *widget;

	if( player->lock_action ) {
		return FALSE;
	}
	/* 如果并没有要使用A/B攻击，则不符合发动条件 */
	if( !player->control.a_attack && !player->control.b_attack ) {
		return FALSE;
	}
	if( !GamePlayerStateCanNormalAttack( player ) ) {
		return FALSE;
	}
	/** 检测终结一击攻击范围内是否有处于喘气状态下的游戏角色 */
	widget = GameObject_GetObjectInAttackRange( 
				player->object,
				ACTION_FINAL_BLOW,
				TRUE, ACTION_REST 
	);
	/* 没有就不发动 */
	if( !widget ) {
		return FALSE;
	}
	return TRUE;
}

/** 开始发动终结一击 */
static void CommonSkill_StartFinalBlow( GamePlayer *player )
{
	void (*func)(LCUI_Widget*);
	/* 若当前是在被举起的状态下，站立着，则改用相应回调函数 */
	if( player->state == STATE_BE_LIFT_STANCE
	|| player->state == STATE_BE_LIFT_SQUAT ) {
		func = GamePlayer_AtBeLiftAttackDone;
	} else {
		func = GamePlayer_AtAttackDone;
	}

	GamePlayer_ChangeState( player, STATE_FINAL_BLOW );
	GameObject_AtActionDone( player->object, ACTION_FINAL_BLOW, func );
	GameObject_ClearAttack( player->object );
	player->attack_type = ATTACK_TYPE_FINAL_BLOW;
	GamePlayer_StopXWalk( player );
	GamePlayer_StopYMotion( player );
	GamePlayer_LockMotion( player );
	GamePlayer_LockAction( player );
}

static void _StartAttack( GamePlayer *player, int state, int action, int attack_type )
{
	void (*func)(LCUI_Widget*);

	if( player->state == STATE_BE_LIFT_STANCE
	|| player->state == STATE_BE_LIFT_SQUAT ) {
		func = GamePlayer_AtBeLiftAttackDone;
	} else {
		func = GamePlayer_AtAttackDone;
	}
	GamePlayer_ChangeState( player, state );
	GameObject_AtActionDone( player->object, action, func );
	player->attack_type = attack_type;
	GamePlayer_StopXWalk( player );
	GamePlayer_StopYMotion( player );
	GamePlayer_LockMotion( player );
	GamePlayer_LockAction( player );
}

/** 检测是否能够使用A攻击 */
static LCUI_BOOL CommonSkill_CanUseAAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanNormalAttack( player );
}

/** 检测是否能够使用B攻击 */
static LCUI_BOOL CommonSkill_CanUseBAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.b_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanNormalAttack( player );
}

/** 检测是否能够使用跳跃A攻击 */
static LCUI_BOOL CommonSkill_CanUseJumpAAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanJumpAttack( player );
}

/** 检测是否能够使用跳跃B攻击 */
static LCUI_BOOL CommonSkill_CanUseJumpBAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.b_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanJumpAttack( player );
}

/** 开始发动A普通攻击 */
static void CommonSkill_StartAAttack( GamePlayer *player )
{
	_StartAttack(	player, STATE_A_ATTACK, 
			ACTION_A_ATTACK, ATTACK_TYPE_PUNCH );
}

/** 开始发动B普通攻击 */
static void CommonSkill_StartBAttack( GamePlayer *player )
{
	_StartAttack(	player, STATE_B_ATTACK, 
			ACTION_B_ATTACK, ATTACK_TYPE_KICK );
}

/** 开始发动高速A攻击 */
static void CommonSkill_StartMachAAttack( GamePlayer *player )
{
	_StartAttack(	player, STATE_MACH_A_ATTACK,
			ACTION_MACH_A_ATTACK, ATTACK_TYPE_PUNCH );
}

/** 开始发动高速B攻击 */
static void CommonSkill_StartMachBAttack( GamePlayer *player )
{
	_StartAttack(	player, STATE_MACH_B_ATTACK, 
			ACTION_MACH_B_ATTACK, ATTACK_TYPE_KICK );
}

/** 检测游戏角色在当前状态下是否能够进行冲撞攻击 */
static LCUI_BOOL GamePlayerStateCanSprintAttack( GamePlayer *player )
{
	if( player->state == STATE_LEFTRUN
	 || player->state == STATE_RIGHTRUN ) {
		return TRUE;
	}
	return FALSE;
}

/** 开始发动冲撞攻击 */
static void _StartSprintAttack( GamePlayer *player, int state, int attack_type )
{
	double acc, speed;
	player->control.run = FALSE;
	if( player->state == STATE_RIGHTRUN ) {
		GameObject_AtXSpeedToZero(
			player->object, -XACC_DASH, 
			GamePlayer_AtAttackDone 
		);
	} else if(player->state == STATE_LEFTRUN ){
		GameObject_AtXSpeedToZero(
			player->object, XACC_DASH, 
			GamePlayer_AtAttackDone 
		);
	} else {
		return;
	}
	GamePlayer_ChangeState( player, state );
	GamePlayer_LockAction( player );
	GamePlayer_LockMotion( player );
	speed = GameObject_GetYSpeed( player->object );
	acc = YSPEED_WALK * XACC_DASH / XSPEED_RUN;
	if( speed < 0.0 ) {
		GameObject_SetYAcc( player->object, acc );
	}
	else if( speed > 0.0 ) {
		GameObject_SetYAcc( player->object, -acc );
	}
	player->attack_type = attack_type;
}

/** 检测是否能够使用冲撞A攻击 */
static LCUI_BOOL CommonSkill_CanUseASprintAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanSprintAttack( player );
}

/** 开始发动冲撞A攻击 */
static void CommonSkill_StartSprintAAttack( GamePlayer *player )
{
	_StartSprintAttack( player, STATE_AS_ATTACK, ATTACK_TYPE_S_PUNCH );
}

/** 检测是否能够使用冲撞B攻击 */
static LCUI_BOOL CommonSkill_CanUseBSprintAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.b_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanSprintAttack( player );
}

/** 开始发动冲撞B攻击 */
static void CommonSkill_StartSprintBAttack( GamePlayer *player )
{
	_StartSprintAttack( player, STATE_BS_ATTACK, ATTACK_TYPE_S_KICK );
}

static LCUI_BOOL CommonSkill_CanUseSprintJumpAAttack( GamePlayer *player )
{
	if( player->lock_action || !player->control.a_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanSprintJumpAttack( player );
}

static LCUI_BOOL CommonSkill_CanUseSprintJumpBAttack( GamePlayer *player )
{
	if( player->lock_action || !player->control.b_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanSprintJumpAttack( player );
}

static void CommonSkill_StartSprintJumpAAttack( GamePlayer *player )
{
	GamePlayer_ChangeState( player, STATE_ASJ_ATTACK );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_SJUMP_PUNCH;
}

static void CommonSkill_StartSprintJumpBAttack( GamePlayer *player )
{
	GamePlayer_ChangeState( player, STATE_BSJ_ATTACK );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_SJUMP_KICK;
}

/** 检测是否可以攻击到躺地玩家 */
static LCUI_BOOL CommonSkill_CanAttackGround( GamePlayer *player )
{
	GamePlayer *other_player;
	other_player = GamePlayer_GetGroundPlayer( player );
	if( other_player ) {
		if( GamePlayer_CanAttackGroundPlayer(player, other_player) ) {
			return TRUE;
		}
	}
	return FALSE;
}

static LCUI_BOOL CommonSkill_CanAAttackGround( GamePlayer *player )
{
	if( !GamePlayerStateCanNormalAttack( player ) ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	return CommonSkill_CanAttackGround( player );
}

static LCUI_BOOL CommonSkill_CanBAttackGround( GamePlayer *player )
{
	if( !GamePlayerStateCanNormalAttack( player ) ) {
		return FALSE;
	}
	if( !player->control.b_attack ) {
		return FALSE;
	}
	return CommonSkill_CanAttackGround( player );
}

/** 检测是否能够举起躺地玩家 */
static LCUI_BOOL CommonSkill_CanLift( GamePlayer *player )
{
	GamePlayer *other_player;
	if( player->lock_action ) {
		return FALSE;
	}
	if( !GamePlayerStateCanNormalAttack( player ) ) {
		return FALSE;
	}
	other_player = GamePlayer_GetGroundPlayer( player );
	if( !other_player ) { 
		return FALSE;
	}
	if( !GamePlayer_CanLiftPlayer( player, other_player ) ) {
		return FALSE;
	}
	/* 记录对方，以便接着举起 */
	player->other = other_player;
	return TRUE;
}

static LCUI_BOOL CommonSkill_CanALift( GamePlayer *player )
{
	if( !player->control.a_attack ) {
		return FALSE;
	}
	return CommonSkill_CanLift( player );
}

static LCUI_BOOL CommonSkill_CanBLift( GamePlayer *player )
{
	if( !player->control.b_attack ) {
		return FALSE;
	}
	return CommonSkill_CanLift( player );
}

/** 被举着，站立 */
static void GamePlayer_SetBeLiftStance( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	if( player->state != STATE_BE_LIFT_SQUAT ) {
		return;
	}
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_BE_LIFT_STANCE );
}

/** 被举着，准备站起 */
static void GamePlayer_BeLiftStartStand( GamePlayer *player )
{
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_BE_LIFT_SQUAT );
	GameObject_AtActionDone( player->object, ACTION_SQUAT, GamePlayer_SetBeLiftStance );
	GamePlayer_LockAction( player );
}

/** 在跳跃结束时 */
static void GamePlayer_AtJumpDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	GamePlayer_UnlockMotion( player );
	GamePlayer_SetReady( player );
}

/** 在着陆完成时 */
static void GamePlayer_AtLandingDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_ResetAttackControl( player );
	GamePlayer_UnlockAction( player );
	GamePlayer_UnlockMotion( player );
	if( player->control.left_motion ) {
		GamePlayer_SetLeftOriented( player );
	}
	else if( player->control.right_motion ) {
		GamePlayer_SetRightOriented( player );
	}
	GamePlayer_ChangeState( player, STATE_JUMP_DONE );
	GamePlayer_LockAction( player );
	GameObject_SetXSpeed( player->object, 0 );
	GameObject_SetYSpeed( player->object, 0 );
	GameObject_AtActionDone( player->object, ACTION_SQUAT, GamePlayer_AtJumpDone );
}

static void GamePlayer_SetFall( GamePlayer *player )
{
	GamePlayer_UnlockAction( player );
	GamePlayer_UnlockMotion( player );
	GamePlayer_ChangeState( player, STATE_JUMP );
	GamePlayer_BreakRest( player );
	GameObject_AtLanding( player->object, 0, -ZACC_JUMP, GamePlayer_AtLandingDone );
	GamePlayer_LockAction( player );
	GamePlayer_LockMotion( player );
}

/** 在被举起的状态下，更新自身的位置 */
static void GamePlayer_UpdateLiftPosition( LCUI_Widget *widget )
{
	double x, y, z;
	GamePlayer *player, *other_player;
	LCUI_Widget *other;

	player = GamePlayer_GetPlayerByWidget( widget );
	other_player = player->other;
	/* 如果没有举起者，或自己不是被举起者 */
	if( !player->other ) {
		GameObject_AtMove( widget, NULL );
		return;
	}
	else if( !player->other->other ) {
		/* 如果举起者并不处于举起状态 */
		if( !GamePlayer_IsInLiftState( player ) ) {
			/* 如果举起者不是要要投掷自己 */
			if( player->state != STATE_THROW ) {
				GamePlayer_SetFall( other_player );
			}
		}
		GameObject_AtMove( widget, NULL );
		return;
	}
	if( !GamePlayer_IsInLiftState( player ) ) {
		if( player->state != STATE_THROW ) {
			GamePlayer_SetFall( other_player );
		}
		GameObject_AtMove( widget, NULL );
		return;
	}
	other = player->other->object;
	x = GameObject_GetX( widget );
	y = GameObject_GetY( widget );
	z = GameObject_GetZ( widget );
	/* 获取当前帧的顶点相对于底线的距离 */
	z += GameObject_GetCurrentFrameTop( widget );
	GameObject_SetZ( other, z );
	GameObject_SetX( other, x );
	GameObject_SetY( other, y );
}

static void GamePlayer_AtLiftDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GameObject_SetZ( player->other->object, LIFT_HEIGHT );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_LIFT_STANCE );
	/* 在举起者的位置变化时更新被举起者的位置 */
	GameObject_AtMove( widget, GamePlayer_UpdateLiftPosition );
}

/** 设置举起另一个角色 */
static void CommonSkill_StartLift( GamePlayer *player )
{
	double x, y;
	int z_index;
	GamePlayer *other_player;

	if( !player->other ) {
		return;
	}
	other_player = player->other;
	/* 对方不处于躺地状态则退出 */
	if( other_player->state != STATE_LYING
	 && other_player->state != STATE_LYING_HIT
	 && other_player->state != STATE_TUMMY
	 && other_player->state != STATE_TUMMY_HIT ) {
		player->other = NULL;
		return;
	}
	
	/* 被举起的角色，需要记录举起他的角色 */
	other_player->other = player;
	GamePlayer_BreakRest( other_player );
	z_index = Widget_GetZIndex( player->object );
	Widget_SetZIndex( other_player->object, z_index+1);
	
	GamePlayer_UnlockAction( other_player );
	if( other_player->state == STATE_LYING
	 || other_player->state == STATE_LYING_HIT ) {
		GamePlayer_ChangeState( other_player, STATE_BE_LIFT_LYING );
	} else {
		GamePlayer_ChangeState( other_player, STATE_BE_LIFT_TUMMY );
	}
	GamePlayer_LockAction( other_player );
	/* 举起前，先停止移动 */
	GamePlayer_StopXMotion( player );
	GamePlayer_StopYMotion( player );
	player->control.left_motion = FALSE;
	player->control.right_motion = FALSE;
	GamePlayer_UnlockAction( player );
	/* 改为下蹲状态 */
	GamePlayer_ChangeState( player, STATE_LIFT_SQUAT );
	GamePlayer_LockAction( player );
	GameObject_AtActionDone( player->object, ACTION_SQUAT, GamePlayer_AtLiftDone );
	GamePlayer_SetRestTimeOut(
		other_player, 
		BE_LIFT_REST_TIMEOUT, 
		GamePlayer_BeLiftStartStand 
	);
	/* 改变躺地角色的坐标 */
	x = GameObject_GetX( player->object );
	y = GameObject_GetY( player->object );
	GameObject_SetX( other_player->object, x );
	GameObject_SetY( other_player->object, y );
	GameObject_SetZ( other_player->object, 20 );
}

static void GamePlayer_AtGroundAttackDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_SQUAT );
	GamePlayer_LockAction( player );
	GameObject_SetXSpeed( player->object, 0 );
	GameObject_SetYSpeed( player->object, 0 );
	GameObject_AtActionDone( player->object, ACTION_SQUAT, GamePlayer_AtAttackDone );
}

static void CommonSkill_JumpTreadStep2( LCUI_Widget *widget )
{
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_JUMP_STOMP );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_JUMP_TREAD;
	GameObject_AtZeroZSpeed( widget, NULL );
	GameObject_ClearAttack( player->object );
}

static void CommonSkill_JumpTreadStep1( LCUI_Widget *widget )
{
	double z_speed, z_acc;
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	z_acc = -ZACC_JUMP;
	z_speed = ZSPEED_JUMP;
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_JUMP );
	GamePlayer_LockAction( player );
	GameObject_AtLanding( widget, z_speed, z_acc, GamePlayer_AtGroundAttackDone );
	GameObject_AtZeroZSpeed( widget, CommonSkill_JumpTreadStep2 );
}

/** 进行跳跃+踩 */
static void CommonSkill_StartJumpTread( GamePlayer *player )
{
	GamePlayer_UnlockAction( player );
	GameObject_SetXAcc( player->object, 0 );
	GamePlayer_ChangeState( player, STATE_SQUAT );
	GamePlayer_LockAction( player );
	GamePlayer_LockMotion( player );
	GameObject_SetXSpeed( player->object, 0 );
	GameObject_SetYSpeed( player->object, 0 );
	GameObject_SetZSpeed( player->object, 0 );
	GameObject_AtActionDone( player->object, ACTION_SQUAT, CommonSkill_JumpTreadStep1 );
}

static void CommonSkill_JumpElbowStep2( LCUI_Widget *widget )
{
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_JUMP_ELBOW );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_JUMP_ELBOW;
	GameObject_AtZeroZSpeed( widget, NULL );
	GameObject_ClearAttack( player->object );
}

static void CommonSkill_JumpElbowStep1( LCUI_Widget *widget )
{
	double z_speed, z_acc;
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	z_acc = -ZACC_JUMP;
	z_speed = ZSPEED_JUMP;
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_JUMP );
	GamePlayer_LockAction( player );
	GameObject_AtLanding( widget, z_speed, z_acc, GamePlayer_AtGroundAttackDone );
	GameObject_AtZeroZSpeed( widget, CommonSkill_JumpElbowStep2 );
}

/** 进行跳跃+肘击 */
void CommonSkill_StartJumpElbow( GamePlayer *player )
{
	GamePlayer_UnlockAction( player );
	GameObject_SetXAcc( player->object, 0 );
	GamePlayer_ChangeState( player, STATE_SQUAT );
	GamePlayer_LockAction( player );
	GamePlayer_LockMotion( player );
	GameObject_SetXSpeed( player->object, 0 );
	GameObject_SetYSpeed( player->object, 0 );
	GameObject_SetZSpeed( player->object, 0 );
	GameObject_ClearAttack( player->object );
	GameObject_AtActionDone( player->object, ACTION_SQUAT, CommonSkill_JumpElbowStep1 );
}

static void GamePlayer_StopMachStomp( GamePlayer *player )
{
	GamePlayer_AtAttackDone( player->object );
}

/** 高速踩踏 */
static void CommonSkill_StartMachStomp( GamePlayer *player )
{
	GamePlayer_ChangeState( player, STATE_MACH_STOMP );
	player->attack_type = ATTACK_TYPE_MACH_STOMP;
	GamePlayer_LockAction( player );
	GamePlayer_LockMotion( player );
	GameObject_SetXSpeed( player->object, 0 );
	GameObject_SetYSpeed( player->object, 0 );
	GamePlayer_SetActionTimeOut( player, 1250, GamePlayer_StopMachStomp );
}

/** 开始发动跳跃A攻击 */
static void CommonSkill_StartJumpAAttack( GamePlayer *player )
{
	GamePlayer_ChangeState( player, STATE_AJ_ATTACK );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_JUMP_PUNCH;
}

/** 开始发动跳跃高速A攻击 */
static void CommonSkill_StartJumpMachAAttack( GamePlayer *player )
{
	GamePlayer_ChangeState( player, STATE_MAJ_ATTACK );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_JUMP_PUNCH;
}

/** 开始发动跳跃B攻击 */
static void CommonSkill_StartJumpBAttack( GamePlayer *player )
{
	GamePlayer_ChangeState( player, STATE_BJ_ATTACK );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_JUMP_KICK;
}

/** 开始发动跳跃高速B攻击 */
static void CommonSkill_StartJumpMachBAttack( GamePlayer *player )
{
	GamePlayer_ChangeState( player, STATE_MBJ_ATTACK );
	GamePlayer_LockAction( player );
	player->attack_type = ATTACK_TYPE_JUMP_KICK;
}

static LCUI_BOOL CommonSkill_CanUseBombKick( GamePlayer *player )
{
	if( player->state != STATE_JUMP_DONE ) {
		return FALSE;
	}
	if( !player->control.b_attack ) {
		return FALSE;
	}
	return TRUE;
}

/** 开始发动 爆裂腿技能 */
static void CommonSkill_StartBombKick( GamePlayer *player )
{
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_BOMBKICK );
	player->attack_type = ATTACK_TYPE_BOMB_KICK;
	GameObject_ClearAttack( player->object );
	GamePlayer_LockAction( player );
	GamePlayer_LockMotion( player );
	if( player->control.left_motion ) {
		GameObject_SetXSpeed( player->object, -100 );
		GamePlayer_SetLeftOriented( player );
	}
	else if( player->control.right_motion ) {
		GameObject_SetXSpeed( player->object, 100 );
		GamePlayer_SetRightOriented( player );
	}
	else if( GamePlayer_IsLeftOriented(player) ) {
		GameObject_SetXSpeed( player->object, -100 );
	} else {
		GameObject_SetXSpeed( player->object, 100 );
	}
	GameObject_AtLanding( player->object, 20, -10, GamePlayer_AtLandingDone );
}

static LCUI_BOOL CommonSkill_CanUseSpinHit( GamePlayer *player )
{
	if( player->state != STATE_JUMP_DONE ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	return TRUE;
}

/** 开始发动 自旋击（翻转击） 技能 */
static void CommonSkill_StartSpinHit( GamePlayer *player )
{
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_SPINHIT );
	player->attack_type = ATTACK_TYPE_SPIN_HIT;
	GamePlayer_LockAction( player );
	GamePlayer_LockMotion( player );
	if( player->control.left_motion ) {
		GameObject_SetXSpeed( player->object, -100 );
		GamePlayer_SetLeftOriented( player );
	}
	else if( player->control.right_motion ) {
		GameObject_SetXSpeed( player->object, 100 );
		GamePlayer_SetRightOriented( player );
	}
	else if( GamePlayer_IsLeftOriented(player) ) {
		GameObject_SetXSpeed( player->object, -100 );
	} else {
		GameObject_SetXSpeed( player->object, 100 );
	}
	GameObject_AtLanding( player->object, 30, -10, GamePlayer_AtLandingDone );
}

static LCUI_BOOL CommonSkill_CanUseAThrow( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanThrow( player );
}

static LCUI_BOOL CommonSkill_CanUseBThrow( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.b_attack ) {
		return FALSE;
	}
	return GamePlayerStateCanThrow( player );
}

/** 在被举起的状态下，被动起跳 */
static void GamePlayer_BeLiftPassiveStartJump( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	player->other = NULL;
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_SJUMP );
}

static void GamePlayer_AtThrowLanding( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_StopXMotion( player );
	GamePlayer_StopYMotion( player );
	GamePlayer_StartStand( player );
}

/** 在抛飞结束时 */
static void GamePlayer_AtThrowUpFlyDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_StopXMotion( player );
	GameObject_AtTouch( widget, NULL );
	if( GamePlayer_SetLying( player ) == 0 ) {
		GamePlayer_SetRestTimeOut(
			player, BE_THROW_REST_TIMEOUT, 
			GamePlayer_StartStand 
		);
	}
}

/** 被抛后落地时，弹起 */
static void GamePlayer_LandingBounce( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_ReduceSpeed( player, 75 );
	Game_RecordAttack( player->other, ATTACK_TYPE_THROW, player, player->state );
	player->other = NULL;
	GameObject_AtLanding(
		widget, ZSPEED_XF_HIT_FLY2, -ZACC_XF_HIT_FLY2,
		GamePlayer_AtThrowUpFlyDone
	);
}

/** 处理游戏角色在被抛出时与其他角色的碰撞 */
static void GamePlayer_ProcThrowUpFlyAttack( LCUI_Widget *self, LCUI_Widget *other )
{
	double x1, x2;
	GamePlayer *player, *other_player;

	player = GamePlayer_GetPlayerByWidget( self );
	other_player = GamePlayer_GetPlayerByWidget( other );
	if( !player || !other_player ) {
		return;
	}

	/* 判断自己的状态是否符合要求 */
	if( player->state != STATE_HIT_FLY
	 && player->state != STATE_HIT_FLY_FALL 
	 && player->state != STATE_LYING_HIT ) {
		return;
	}

	if( GamePlayer_TryHit( other_player ) == 0 ) {
		return;
	}
	x1 = GameObject_GetX( self );
	x2 = GameObject_GetX( other );
	/* 记录攻击 */
	Game_RecordAttack(	player->other, ATTACK_TYPE_BUMPED,
				other_player, other_player->state );
	/* 根据两者坐标，判断击飞的方向 */
	if( x1 < x2 ) {
		GamePlayer_SetRightHitFly( other_player );
	} else {
		GamePlayer_SetLeftHitFly( other_player );
	}
	other_player->n_attack = 0;
}

/** 在抛投动作结束时 */
static void GamePlayer_AtThrowDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	double z, z_speed, z_acc;

	player = GamePlayer_GetPlayerByWidget( widget );
	z = GameObject_GetZ( widget );
	z_speed = GameObject_GetZSpeed( widget );
	z_acc = GameObject_GetZAcc( widget );
	/* 若还未落地，则维持该动作，直至落地时切换 */
	if( z > 0 && z_acc < 0 ) {
		GameObject_AtLanding( widget, z_speed, z_acc, GamePlayer_AtThrowLanding );
	} else {
		GamePlayer_StopXMotion( player );
		GamePlayer_StopYMotion( player );
		GamePlayer_UnlockAction( player );
		GamePlayer_SetReady( player );
	}
}

static void GamePlayer_AtBeThrowDownDone( LCUI_Widget *widget )
{
	int ret = 0;
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	switch( player->state ) {
	case STATE_TUMMY:
	case STATE_TUMMY_HIT:
		ret = GamePlayer_SetTummy( player );
		break;
	case STATE_LYING:
	case STATE_LYING_HIT:
		ret = GamePlayer_SetLying( player );
		break;
	default:
		break;
	}
	GamePlayer_LockAction( player );
	GamePlayer_StopXMotion( player );
	if( ret == 0 ) {
		GamePlayer_SetRestTimeOut( player, BE_THROW_REST_TIMEOUT, GamePlayer_StartStand );
	}
}

static void GamePlayer_AtBeThrowDownLanding( LCUI_Widget *widget )
{
	GamePlayer *player;

	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	if( player->state == STATE_BE_LIFT_TUMMY ) {
		GamePlayer_ChangeState( player, STATE_TUMMY_HIT );
	} else {
		GamePlayer_ChangeState( player, STATE_LYING_HIT );
	}
	GamePlayer_LockAction( player );
	Game_RecordAttack( player->other, ATTACK_TYPE_THROW, player, player->state );
	player->other = NULL;
	/* 落地后缩减该角色75%的移动速度 */
	GamePlayer_ReduceSpeed( player, 75 );
	GameObject_AtLanding(
		player->object, ZSPEED_XF_HIT_FLY2,
		-ZACC_XF_HIT_FLY2, GamePlayer_AtBeThrowDownDone
	);
}

/** 将举起的角色向下摔 */
static void CommonSkill_StartAThrow( GamePlayer *player )
{
	double x_speed;
	if( !GamePlayer_IsInLiftState(player)
	|| player->state == STATE_LIFT_SQUAT ) {
		return;
	}
	if( player->state == STATE_LIFT_RUN ) {
		if( GamePlayer_IsLeftOriented(player) ) {
			GameObject_SetXAcc( player->object, XACC_STOPRUN );
		} else {
			GameObject_SetXAcc( player->object, -XACC_STOPRUN );
		}
	}
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_THROW );
	GameObject_AtActionDone( player->object, ACTION_THROW, GamePlayer_AtThrowDone );
	if( player->other ) {
		GamePlayer_BreakRest( player->other );
		GamePlayer_UnlockAction( player->other );
		switch( player->other->state ) {
		case STATE_BE_LIFT_LYING:
		case STATE_BE_LIFT_LYING_HIT:
		case STATE_BE_LIFT_TUMMY:
		case STATE_BE_LIFT_TUMMY_HIT:
			/* 如果被举起的角色还处于 躺/趴着 的状态，
			 * 那就根据举起者的方向，将被举起者向相应方向扔。
			 */
			if( GamePlayer_IsLeftOriented(player) ) {
				x_speed = -XSPEED_THROWDOWN_FLY;
				GamePlayer_SetRightOriented( player->other );
			} else {
				x_speed = XSPEED_THROWDOWN_FLY;
				GamePlayer_SetLeftOriented( player->other );
			}
			x_speed += GameObject_GetXSpeed( player->object )/2;
			GamePlayer_LockMotion( player->other );
			GameObject_SetXSpeed( player->other->object, x_speed );
			GameObject_AtLanding(
				player->other->object, -ZSPEED_THROWDOWN_FLY,
				-ZACC_THROWDOWN_FLY, GamePlayer_AtBeThrowDownLanding
			);
			break;
		case STATE_SQUAT:
		case STATE_BE_LIFT_SQUAT:
		case STATE_BE_LIFT_STANCE:
			if( GamePlayer_IsLeftOriented(player) ) {
				x_speed = -XSPEED_RUN/2;
			} else {
				x_speed = XSPEED_RUN/2;
			}
			x_speed += GameObject_GetXSpeed( player->object )/4;
			GamePlayer_LockMotion( player->other );
			GameObject_SetXSpeed( player->other->object, x_speed );
			GamePlayer_ChangeState( player->other, STATE_BE_LIFT_SQUAT );
			GameObject_AtLanding( 
				player->other->object, 
				ZSPEED_JUMP, -ZACC_JUMP, 
				GamePlayer_AtLandingDone
			);
			GameObject_AtActionDone(
				player->other->object, ACTION_SQUAT,
				GamePlayer_BeLiftPassiveStartJump 
			);
		default:
			break;
		}
		GamePlayer_LockAction( player->other );
	}
	GamePlayer_LockAction( player );
	/* 解除举起者与被举起者的关系 */
	//player->other->other = NULL;
	player->other = NULL;
}

/** 将举起的角色向前抛出 */
static void CommonSkill_StartBThrow( GamePlayer *player )
{
	double x_speed;
	if( !GamePlayer_IsInLiftState(player)
	|| player->state == STATE_LIFT_SQUAT ) {
		return;
	}
	/* 如果在跑步，那就减点速度 */
	if( player->state == STATE_LIFT_RUN ) {
		if( GamePlayer_IsLeftOriented(player) ) {
			GameObject_SetXAcc( player->object, XACC_STOPRUN );
		} else {
			GameObject_SetXAcc( player->object, -XACC_STOPRUN );
		}
	}
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_THROW );
	GameObject_AtActionDone( player->object, ACTION_THROW, GamePlayer_AtThrowDone );
	if( player->other ) {
		/* 打断休息时限倒计时，避免角色被抛出时，在空中站起 */
		GamePlayer_BreakRest( player->other );
		GamePlayer_UnlockAction( player->other );
		switch( player->other->state ) {
		case STATE_BE_LIFT_LYING:
		case STATE_BE_LIFT_LYING_HIT:
		case STATE_BE_LIFT_TUMMY:
		case STATE_BE_LIFT_TUMMY_HIT:
			/* 如果被举起的角色还处于 躺/趴着 的状态，
			 * 那就根据举起者的方向，将被举起者向相应方向扔。
			 */
			if( GamePlayer_IsLeftOriented(player) ) {
				x_speed = -XSPEED_THROWUP_FLY;
				GamePlayer_SetRightOriented( player->other );
			} else {
				x_speed = XSPEED_THROWUP_FLY;
				GamePlayer_SetLeftOriented( player->other );
			}
			x_speed += GameObject_GetXSpeed( player->object )/4;
			GamePlayer_LockMotion( player->other );
			GameObject_SetXSpeed( player->other->object, x_speed );
			GamePlayer_ChangeState( player->other, STATE_HIT_FLY_FALL );
			GameObject_AtLanding(
				player->other->object, ZSPEED_THROWUP_FLY, 
				-ZACC_THROWUP_FLY, GamePlayer_LandingBounce
			);
			GameObject_AtTouch(
				player->other->object,
				GamePlayer_ProcThrowUpFlyAttack 
			);
			break;
		case STATE_SQUAT:
		case STATE_BE_LIFT_SQUAT:
		case STATE_BE_LIFT_STANCE:
			if( GamePlayer_IsLeftOriented(player) ) {
				x_speed = -XSPEED_RUN/2;
			} else {
				x_speed = XSPEED_RUN/2;
			}
			x_speed += GameObject_GetXSpeed( player->object )/2;
			GamePlayer_LockMotion( player->other );
			GameObject_SetXSpeed( player->other->object, x_speed );
			GamePlayer_ChangeState( player->other, STATE_BE_LIFT_SQUAT );
			GameObject_AtLanding( 
				player->other->object, 
				ZSPEED_JUMP, -ZACC_JUMP, 
				GamePlayer_AtLandingDone
			);
			GameObject_AtActionDone(
				player->other->object, ACTION_SQUAT,
				GamePlayer_BeLiftPassiveStartJump 
			);
		default:
			break;
		}
		GamePlayer_LockAction( player->other );
	}
	GamePlayer_LockAction( player );
	/* 解除举起者与被举起者的关系 */
	//player->other->other = NULL;
	player->other = NULL;
}

static LCUI_BOOL CommonSkill_CanUseRide( GamePlayer *player )
{
	GamePlayer *other_player;
	if( player->lock_motion ) {
		return FALSE;
	}
	if( !player->control.up_motion
	 && !player->control.down_motion ) {
		 return FALSE;
	}
	if( player->state != STATE_WALK ) {
		return FALSE;
	}
	other_player = GamePlayer_GetGroundPlayer( player );
	if( !other_player ) {
		return FALSE;
	}
	if( !GamePlayer_CanLiftPlayer( player, other_player ) ) {
		return FALSE;
	}
	player->other = other_player;
	return TRUE;
}

static void CommonSkill_StartRide( GamePlayer *player )
{
	int z_index;
	double x, y;
	if( player->other ) {
		player->other->other = player;
	} else {
		return;
	}
	GamePlayer_StopXMotion( player );
	GamePlayer_StopYMotion( player );
	x = GameObject_GetX( player->other->object );
	y = GameObject_GetY( player->other->object );
	GameObject_SetX( player->object, x );
	GameObject_SetY( player->object, y );
	GamePlayer_ChangeState( player, STATE_RIDE );
	z_index = Widget_GetZIndex( player->other->object );
	Widget_SetZIndex( player->object, z_index+1 );
}

static LCUI_BOOL CommonSkill_CanUseCatch( GamePlayer *player )
{
	GamePlayer *other_player;
	if( player->lock_motion ) {
		return FALSE;
	}
	switch( player->state ) {
	case STATE_READY:
	case STATE_STANCE:
	case STATE_WALK:
		other_player = GamePlayer_CatchGaspingPlayer( player );
		if( other_player ) {
			player->other = other_player;
			return TRUE;
		}
	default:
		break;
	}
	return FALSE;
}

static void GamePlayer_AtCatchDone( GamePlayer *player )
{
	GamePlayer_UnlockAction( player );
	GamePlayer_SetReady( player );
	if( player->other ) {
		GamePlayer_UnlockAction( player->other );
		GamePlayer_SetRest( player->other );
	}
}

static void CommonSkill_StartCatch( GamePlayer *player )
{
	double x, y;
	if( !player->other ) {
		return;
	}
	GamePlayer_UnlockAction( player );
	GamePlayer_UnlockAction( player->other );
	GamePlayer_StopXMotion( player );
	GamePlayer_StopYMotion( player );
	x = GameObject_GetX( player->object );
	y = GameObject_GetY( player->object );
	GameObject_SetY( player->other->object, y );
	if( player->control.left_motion && player->control.right_motion ) {
		if( GamePlayer_IsLeftOriented(player->other) ) {
			if( x < GameObject_GetX( player->other->object ) ) {
				return;
			}
			if( GamePlayer_IsLeftOriented(player->other) ) {
				GamePlayer_ChangeState( player->other, STATE_BACK_BE_CATCH );
			} else {
				GamePlayer_ChangeState( player->other, STATE_BE_CATCH );
			}
			GameObject_SetX( player->other->object, x-30 );
		} else {
			if( x > GameObject_GetX( player->other->object ) ) {
				return;
			}
			if( GamePlayer_IsLeftOriented(player->other) ) {
				GamePlayer_ChangeState( player->other, STATE_BE_CATCH );
			} else {
				GamePlayer_ChangeState( player->other, STATE_BACK_BE_CATCH );
			}
			GameObject_SetX( player->other->object, x+30 );
		}
	}
	else if( player->control.left_motion && !player->control.right_motion ) {
		if( GamePlayer_IsLeftOriented(player->other) ) {
			GamePlayer_ChangeState( player->other, STATE_BACK_BE_CATCH );
		} else {
			GamePlayer_ChangeState( player->other, STATE_BE_CATCH );
		}
		GameObject_SetX( player->other->object, x-30 );
	}
	else if( player->control.right_motion && !player->control.left_motion ) {
		if( GamePlayer_IsLeftOriented(player->other) ) {
			GamePlayer_ChangeState( player->other, STATE_BE_CATCH );
		} else {
			GamePlayer_ChangeState( player->other, STATE_BACK_BE_CATCH );
		}
		GameObject_SetX( player->other->object, x+30 );
	} else {
		return;
	}
	GamePlayer_ChangeState( player, STATE_CATCH );
	GamePlayer_LockAction( player );
	GamePlayer_LockAction( player->other );
	GamePlayer_LockMotion( player );
	GamePlayer_LockMotion( player->other );
	GamePlayer_SetActionTimeOut( player, 2000, GamePlayer_AtCatchDone );
}

/** 注册A攻击 */
static void CommonSkill_RegisterAAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_A_ATTACK, 
				SKILLPRIORITY_A_ATTACK,
				CommonSkill_CanUseAAttack,
				CommonSkill_StartAAttack
	);
	SkillLibrary_AddSkill(	SKILLNAME_MACH_A_ATTACK, 
				SKILLPRIORITY_MACH_A_ATTACK,
				CommonSkill_CanUseAAttack,
				CommonSkill_StartMachAAttack
	);
}

/** 注册B攻击 */
static void CommonSkill_RegisterBAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_B_ATTACK, 
				SKILLPRIORITY_B_ATTACK,
				CommonSkill_CanUseBAttack,
				CommonSkill_StartBAttack
	);
	SkillLibrary_AddSkill(	SKILLNAME_MACH_B_ATTACK, 
				SKILLPRIORITY_MACH_B_ATTACK,
				CommonSkill_CanUseBAttack,
				CommonSkill_StartMachBAttack
	);
}

/** 注册跳跃A攻击 */
static void CommonSkill_RegisterJumpAAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_JUMP_A_ATTACK, 
				SKILLPRIORITY_JUMP_A_ATTACK,
				CommonSkill_CanUseJumpAAttack,
				CommonSkill_StartJumpAAttack
	);
	SkillLibrary_AddSkill(	SKILLNAME_JUMP_MACH_A_ATTACK, 
				SKILLPRIORITY_JUMP_MACH_A_ATTACK,
				CommonSkill_CanUseJumpAAttack,
				CommonSkill_StartJumpMachAAttack
	);
}

/** 注册跳跃B攻击 */
static void CommonSkill_RegisterJumpBAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_JUMP_B_ATTACK, 
				SKILLPRIORITY_JUMP_B_ATTACK,
				CommonSkill_CanUseJumpBAttack,
				CommonSkill_StartJumpBAttack
	);
	SkillLibrary_AddSkill(	SKILLNAME_JUMP_MACH_B_ATTACK, 
				SKILLPRIORITY_JUMP_MACH_B_ATTACK,
				CommonSkill_CanUseJumpBAttack,
				CommonSkill_StartJumpMachBAttack
	);
}

/** 注册冲刺跳跃A攻击 */
static void CommonSkill_RegisterSprintJumpAAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_SPRINT_JUMP_A_ATTACK, 
				SKILLPRIORITY_SPRINT_JUMP_A_ATTACK,
				CommonSkill_CanUseSprintJumpAAttack,
				CommonSkill_StartSprintJumpAAttack
	);
}

/** 注册冲刺跳跃B攻击 */
static void CommonSkill_RegisterSprintJumpBAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_SPRINT_JUMP_B_ATTACK, 
				SKILLPRIORITY_SPRINT_JUMP_B_ATTACK,
				CommonSkill_CanUseSprintJumpBAttack,
				CommonSkill_StartSprintJumpBAttack
	);
}

/** 注册 终结一击 技能 */
static void CommonSkill_RegisterFinalBlow(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_A_FINALBLOW, 
				SKILLPRIORITY_A_FINALBLOW,
				CommonSkill_CanUseFinalBlow,
				CommonSkill_StartFinalBlow
	);
	SkillLibrary_AddSkill(	SKILLNAME_B_FINALBLOW, 
				SKILLPRIORITY_B_FINALBLOW,
				CommonSkill_CanUseFinalBlow,
				CommonSkill_StartFinalBlow
	);
}

static void CommonSkill_RegisterSprintAAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_SPRINT_A_ATTACK,
				SKILLPRIORITY_SPRINT_A_ATTACK,
				CommonSkill_CanUseASprintAttack,
				CommonSkill_StartSprintAAttack
	);
}

static void CommonSkill_RegisterSprintBAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_SPRINT_B_ATTACK,
				SKILLPRIORITY_SPRINT_B_ATTACK,
				CommonSkill_CanUseBSprintAttack,
				CommonSkill_StartSprintBAttack
	);
}

static void CommonSkill_RegisterMachStomp(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_MACH_STOMP,
				SKILLPRIORITY_MACH_STOMP,
				CommonSkill_CanAAttackGround,
				CommonSkill_StartMachStomp
	);
}

static void CommonSkill_RegisterJumpElbow(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_JUMP_ELBOW,
				SKILLPRIORITY_JUMP_ELBOW,
				CommonSkill_CanAAttackGround,
				CommonSkill_StartJumpElbow
	);
}

static void CommonSkill_RegisterJumpTread(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_JUMP_TREAD,
				SKILLPRIORITY_JUMP_TREAD,
				CommonSkill_CanBAttackGround,
				CommonSkill_StartJumpTread
	);
}

static void CommonSkill_RegisterLift(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_A_LIFT,
				SKILLPRIORITY_A_LIFT,
				CommonSkill_CanALift,
				CommonSkill_StartLift
	);
	SkillLibrary_AddSkill(	SKILLNAME_B_LIFT,
				SKILLPRIORITY_B_LIFT,
				CommonSkill_CanBLift,
				CommonSkill_StartLift
	);
}

/** 注册 爆裂腿技能 */
static void CommonSkill_RegisterBombKick(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_BOMBKICK,
				SKILLPRIORITY_BOMBKICK,
				CommonSkill_CanUseBombKick,
				CommonSkill_StartBombKick
	);
}

/** 注册 爆裂腿技能 */
static void CommonSkill_RegisterSpinHit(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_SPINHIT,
				SKILLPRIORITY_SPINHIT,
				CommonSkill_CanUseSpinHit,
				CommonSkill_StartSpinHit
	);
}

static void CommonSkill_RegisterThrow(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_A_THROW,
				SKILLPRIORITY_A_THROW,
				CommonSkill_CanUseAThrow,
				CommonSkill_StartAThrow
	);
	SkillLibrary_AddSkill(	SKILLNAME_B_THROW,
				SKILLPRIORITY_B_THROW,
				CommonSkill_CanUseBThrow,
				CommonSkill_StartBThrow
	);
}

static void CommonSkill_RegisterRide(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_RIDE,
				SKILLPRIORITY_RIDE,
				CommonSkill_CanUseRide,
				CommonSkill_StartRide
	);
}

static LCUI_BOOL CommonSkill_CanUseRideAAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.a_attack ) {
		return FALSE;
	}
	if( !player->state == STATE_RIDE ) {
		return FALSE;
	}
	return TRUE;
}

static LCUI_BOOL CommonSkill_CanUseRideBAttack( GamePlayer *player )
{
	if( player->lock_action ) {
		return FALSE;
	}
	if( !player->control.b_attack ) {
		return FALSE;
	}
	if( !player->state == STATE_RIDE ) {
		return FALSE;
	}
	return TRUE;
}

static void GamePlayer_AtRideAttackDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	GamePlayer_UnlockAction( player );
	/* 如果还骑在对方身上 */
	if( player->other ) {
		GamePlayer_ChangeState( player, STATE_RIDE );
	}
}

static void CommonSkill_StartRideAAttack( GamePlayer *player )
{
	if( !player->other ) {
		return;
	}
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_RIDE_ATTACK );
	GamePlayer_LockAction( player );
	GamePlayer_TryHit( player->other );
	Game_RecordAttack( player, ATTACK_TYPE_RIDE_ATTACK, player->other, player->other->state );
	GameObject_AtActionDone( player->object, ACTION_RIDE_ATTACK, GamePlayer_AtRideAttackDone );
}

static void GamePlayer_AtRideJumpDone( LCUI_Widget *widget )
{
	GamePlayer *player;
	player = GamePlayer_GetPlayerByWidget( widget );
	if( !player->other ) {
		GamePlayer_StartStand( player );
		return;
	}
	switch( player->other->state ) {
	case STATE_TUMMY:
	case STATE_TUMMY_HIT:
	case STATE_LYING:
	case STATE_LYING_HIT:
		Game_RecordAttack(	player, ATTACK_TYPE_RIDE_JUMP_ATTACK, 
					player->other, player->other->state );
		CommonSkill_StartRideAAttack( player );
		break;
	default:
		player->other = NULL;
		GamePlayer_StartStand( player );
		break;
	}
}

static void CommonSkill_StartRideBAttack( GamePlayer *player )
{
	if( !player->other ) {
		return;
	}
	GamePlayer_UnlockAction( player );
	GamePlayer_ChangeState( player, STATE_RIDE_JUMP );
	GamePlayer_LockAction( player );
	GameObject_AtLanding( player->object, ZSPEED_JUMP, -ZACC_JUMP, GamePlayer_AtRideJumpDone );
}

static void CommonSkill_RegisterRideAttack(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_RIDE_A_ATTACK,
				SKILLPRIORITY_RIDE_A_ATTACK,
				CommonSkill_CanUseRideAAttack,
				CommonSkill_StartRideAAttack
	);
	SkillLibrary_AddSkill(	SKILLNAME_RIDE_B_ATTACK,
				SKILLPRIORITY_RIDE_B_ATTACK,
				CommonSkill_CanUseRideBAttack,
				CommonSkill_StartRideBAttack
	);
}

static void CommonSkill_RegisterCatch(void)
{
	SkillLibrary_AddSkill(	SKILLNAME_CATCH,
				SKILLPRIORITY_CATCH,
				CommonSkill_CanUseCatch,
				CommonSkill_StartCatch
	);
}

/** 注册通用技能 */
void CommonSkill_Register(void)
{
	CommonSkill_RegisterAAttack();
	CommonSkill_RegisterBAttack();
	CommonSkill_RegisterJumpAAttack();
	CommonSkill_RegisterJumpBAttack();
	CommonSkill_RegisterSprintAAttack();
	CommonSkill_RegisterSprintBAttack();
	CommonSkill_RegisterSprintJumpAAttack();
	CommonSkill_RegisterSprintJumpBAttack();
	CommonSkill_RegisterFinalBlow();
	CommonSkill_RegisterJumpElbow();
	CommonSkill_RegisterJumpTread();
	CommonSkill_RegisterLift();
	CommonSkill_RegisterThrow();
	CommonSkill_RegisterMachStomp();
	CommonSkill_RegisterBombKick();
	CommonSkill_RegisterSpinHit();
	CommonSkill_RegisterRide();
	CommonSkill_RegisterRideAttack();
	CommonSkill_RegisterCatch();
}