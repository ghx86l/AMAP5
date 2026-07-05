#include "PPR_BulletWorld.h"
#include "PPR_AnimNode.h"

THIRD_PARTY_INCLUDES_START
#include <btBulletDynamicsCommon.h>
THIRD_PARTY_INCLUDES_END

class FKinematicMotionState : public btMotionState
{
public:
    btTransform m_Transform;
    explicit FKinematicMotionState(const btTransform& T) : m_Transform(T) {}
    void getWorldTransform(btTransform& Out) const override { Out = m_Transform; }
    void setWorldTransform(const btTransform&) override {}
};

class FDynamicMotionState : public btMotionState
{
public:
    btTransform m_Transform;
    explicit FDynamicMotionState(const btTransform& T) : m_Transform(T) {}
    void getWorldTransform(btTransform& Out) const override { Out = m_Transform; }
    void setWorldTransform(const btTransform& T) override { m_Transform = T; }
};

static btTransform UEToBt(const FTransform& T)
{
    const FQuat Q = T.GetRotation();
    const FVector P = T.GetLocation();
    return btTransform(
        btQuaternion((float)Q.X, (float)Q.Y, (float)Q.Z, (float)Q.W),
        btVector3((float)P.X, (float)P.Y, (float)P.Z));
}

static FTransform BtToUE(const btTransform& T)
{
    const btQuaternion Q = T.getRotation();
    const btVector3    P = T.getOrigin();
    return FTransform(
        FQuat((float)Q.x(), (float)Q.y(), (float)Q.z(), (float)Q.w()),
        FVector((float)P.x(), (float)P.y(), (float)P.z()),
        FVector::OneVector);
}
static btTransform JointToBt(const FMmdJointData& Joint)
{
    const FQuat Rotation = Joint.Rotation.GetNormalized();
    return btTransform(
        btQuaternion((float)Rotation.X, (float)Rotation.Y, (float)Rotation.Z, (float)Rotation.W),
        btVector3((float)Joint.Position.X, (float)Joint.Position.Y, (float)Joint.Position.Z));
}

static TArray<EMmdRigidBodyMode> BuildEffectiveModes(
    const TArray<FMmdRigidBodyData>& InRigidBodies,
    const TArray<FMmdJointData>& InJoints)
{
    TArray<EMmdRigidBodyMode> EffectiveModes;
    EffectiveModes.Reserve(InRigidBodies.Num());
    for (const FMmdRigidBodyData& RB : InRigidBodies)
    {
        EffectiveModes.Add(RB.Mode);
    }

    auto TryPromoteMergeBody = [&](int32 ParentRigidBodyIndex, int32 ChildRigidBodyIndex) -> void
    {
        if (!EffectiveModes.IsValidIndex(ParentRigidBodyIndex) ||
            !EffectiveModes.IsValidIndex(ChildRigidBodyIndex))
        {
            return;
        }

        if (EffectiveModes[ParentRigidBodyIndex] == EMmdRigidBodyMode::FollowBone ||
            EffectiveModes[ChildRigidBodyIndex] != EMmdRigidBodyMode::PhysicsAndBoneMerge)
        {
            return;
        }

        const FMmdRigidBodyData& ParentBody = InRigidBodies[ParentRigidBodyIndex];
        const FMmdRigidBodyData& ChildBody = InRigidBodies[ChildRigidBodyIndex];
        if (ParentBody.ResolvedBoneIndex < 0 || ChildBody.ResolvedBoneIndex < 0)
        {
            return;
        }

        if (ChildBody.ParentBoneIndex == ParentBody.ResolvedBoneIndex)
        {
            EffectiveModes[ChildRigidBodyIndex] = EMmdRigidBodyMode::PhysicsOnly;
        }
    };

    for (const FMmdJointData& Joint : InJoints)
    {
        if (!InRigidBodies.IsValidIndex(Joint.RigidBodyIndexA) ||
            !InRigidBodies.IsValidIndex(Joint.RigidBodyIndexB) ||
            Joint.RigidBodyIndexA == Joint.RigidBodyIndexB)
        {
            continue;
        }

        TryPromoteMergeBody(Joint.RigidBodyIndexA, Joint.RigidBodyIndexB);
        TryPromoteMergeBody(Joint.RigidBodyIndexB, Joint.RigidBodyIndexA);
    }

    return EffectiveModes;
}


struct FBulletWorld::FImpl
{
    btDefaultCollisionConfiguration*        CollisionConfig = nullptr;
    btCollisionDispatcher*                  Dispatcher      = nullptr;
    btDbvtBroadphase*                       Broadphase      = nullptr;
    btSequentialImpulseConstraintSolver*    Solver          = nullptr;
    btDiscreteDynamicsWorld*                World           = nullptr;

    struct FEntry
    {
        btCollisionShape*   Shape         = nullptr;
        btMotionState*      MotionState   = nullptr;
        btRigidBody*        Body          = nullptr;
        EMmdRigidBodyMode   EffectiveMode = EMmdRigidBodyMode::FollowBone;
    };

    struct FJointEntry
    {
        btTypedConstraint* Constraint = nullptr;
    };

    TArray<FEntry> Entries;
    TArray<FJointEntry> JointEntries;
};

FBulletWorld::FBulletWorld() {}
FBulletWorld::~FBulletWorld() { Shutdown(); }

void FBulletWorld::Initialize(const TArray<FMmdRigidBodyData>& InRigidBodies,
                              const TArray<FMmdJointData>& InJoints,
                              float InERP,
                              const FVector& InGravityDirection)
{
    Shutdown();
    Impl = new FImpl();

    Impl->CollisionConfig = new btDefaultCollisionConfiguration();
    Impl->Dispatcher      = new btCollisionDispatcher(Impl->CollisionConfig);
    Impl->Broadphase      = new btDbvtBroadphase();
    Impl->Solver          = new btSequentialImpulseConstraintSolver();
    Impl->World           = new btDiscreteDynamicsWorld(
        Impl->Dispatcher, Impl->Broadphase, Impl->Solver, Impl->CollisionConfig);
    Impl->World->setGravity(btVector3(
        (float)InGravityDirection.X * 980.f,
        (float)InGravityDirection.Y * 980.f,
        (float)InGravityDirection.Z * 980.f));
    Impl->World->getSolverInfo().m_erp = InERP;
    const TArray<EMmdRigidBodyMode> EffectiveModes = BuildEffectiveModes(InRigidBodies, InJoints);

    for (int32 RigidBodyIndex = 0; RigidBodyIndex < InRigidBodies.Num(); ++RigidBodyIndex)
    {
        const FMmdRigidBodyData& RB = InRigidBodies[RigidBodyIndex];
        const EMmdRigidBodyMode EffectiveMode = EffectiveModes.IsValidIndex(RigidBodyIndex)
            ? EffectiveModes[RigidBodyIndex]
            : RB.Mode;

        btCollisionShape* Shape = nullptr;
        bool bZeroVolume = false;

        switch (RB.ShapeType)
        {
        case EMmdRigidBodyShape::Sphere:
            Shape = new btSphereShape((float)RB.ShapeSize.X);
            bZeroVolume = (RB.ShapeSize.X == 0.f);
            break;
        case EMmdRigidBodyShape::Box:
            Shape = new btBoxShape(btVector3(
                (float)RB.ShapeSize.X, (float)RB.ShapeSize.Y, (float)RB.ShapeSize.Z));
            bZeroVolume = (RB.ShapeSize.X == 0.f || RB.ShapeSize.Y == 0.f || RB.ShapeSize.Z == 0.f);
            break;
        case EMmdRigidBodyShape::Capsule:
            Shape = new btCapsuleShapeZ((float)RB.ShapeSize.X, (float)RB.ShapeSize.Y);
            bZeroVolume = (RB.ShapeSize.X == 0.f || RB.ShapeSize.Y == 0.f);
            break;
        default:
            Shape = new btSphereShape(1.f);
        }

        Shape->setMargin(0.01f);

        const btTransform InitialT = UEToBt(RB.CachedUETransform);
        const bool bKinematic      = (EffectiveMode == EMmdRigidBodyMode::FollowBone);
        const float Mass           = bKinematic ? 0.f : (float)RB.Mass;

        btMotionState* MotionState = bKinematic
            ? (btMotionState*)new FKinematicMotionState(InitialT)
            : (btMotionState*)new FDynamicMotionState(InitialT);

        btVector3 Inertia(0, 0, 0);
        if (Mass > 0.f)
            Shape->calculateLocalInertia(Mass, Inertia);

        btRigidBody::btRigidBodyConstructionInfo Info(Mass, MotionState, Shape, Inertia);
        Info.m_linearDamping     = (float)RB.LinearDamping;
        Info.m_angularDamping    = (float)RB.AngularDamping;
        Info.m_restitution       = (float)RB.Restitution;
        Info.m_friction          = (float)RB.Friction;
        Info.m_additionalDamping = true;

        btRigidBody* Body = new btRigidBody(Info);

        Body->setSleepingThresholds(0.01f, 0.1f * 3.14159265358979323846f / 180.0f);
        Body->setActivationState(DISABLE_DEACTIVATION);

        if (bKinematic)
            Body->setCollisionFlags(Body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);

        const bool bNoContact = bZeroVolume || ((uint16)RB.PassGroup == 0x0000);
        if (bNoContact)
            Body->setCollisionFlags(Body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);

        // CCD はNexGima バニラでは無効 (標準Bullet挙動に従う)

        const int Group     = 1 << RB.Group;
        const int GroupMask = (int)(uint16)RB.PassGroup;
        Impl->World->addRigidBody(Body, Group, GroupMask);

        Body->setLinearVelocity(btVector3(0.f, 0.f, 0.f));
        Body->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
        Body->setInterpolationLinearVelocity(btVector3(0.f, 0.f, 0.f));
        Body->setInterpolationAngularVelocity(btVector3(0.f, 0.f, 0.f));
        Body->setInterpolationWorldTransform(InitialT);
        Body->clearForces();

        FImpl::FEntry Entry;
        Entry.Shape         = Shape;
        Entry.MotionState   = MotionState;
        Entry.Body          = Body;
        Entry.EffectiveMode = EffectiveMode;
        Impl->Entries.Add(Entry);
    }

    for (const FMmdJointData& Joint : InJoints)
    {
        FImpl::FJointEntry JointEntry;

        if (!Impl->Entries.IsValidIndex(Joint.RigidBodyIndexA) ||
            !Impl->Entries.IsValidIndex(Joint.RigidBodyIndexB) ||
            Joint.RigidBodyIndexA == Joint.RigidBodyIndexB)
        {
            Impl->JointEntries.Add(JointEntry);
            continue;
        }

        btRigidBody* BodyA = Impl->Entries[Joint.RigidBodyIndexA].Body;
        btRigidBody* BodyB = Impl->Entries[Joint.RigidBodyIndexB].Body;
        if (!BodyA || !BodyB)
        {
            Impl->JointEntries.Add(JointEntry);
            continue;
        }

        const btTransform JointTransform = JointToBt(Joint);
        const btTransform FrameInA = BodyA->getWorldTransform().inverse() * JointTransform;
        const btTransform FrameInB = BodyB->getWorldTransform().inverse() * JointTransform;

        switch (Joint.Kind)
        {
        case EMmdJointKind::Spring6Dof:
        {
            btGeneric6DofSpringConstraint* Constraint =
                new btGeneric6DofSpringConstraint(*BodyA, *BodyB, FrameInA, FrameInB, true);
            Constraint->setLinearLowerLimit(btVector3(
                (float)Joint.PositionMin.X,
                (float)Joint.PositionMin.Y,
                (float)Joint.PositionMin.Z));
            Constraint->setLinearUpperLimit(btVector3(
                (float)Joint.PositionMax.X,
                (float)Joint.PositionMax.Y,
                (float)Joint.PositionMax.Z));
            Constraint->setAngularLowerLimit(btVector3(
                (float)Joint.RotationMin.X,
                (float)Joint.RotationMin.Y,
                (float)Joint.RotationMin.Z));
            Constraint->setAngularUpperLimit(btVector3(
                (float)Joint.RotationMax.X,
                (float)Joint.RotationMax.Y,
                (float)Joint.RotationMax.Z));
            if (!FMath::IsNearlyZero(Joint.SpringPosition.X))
            {
                Constraint->enableSpring(0, true);
                Constraint->setStiffness(0, (float)Joint.SpringPosition.X);
                Constraint->setDamping(0, 0.5f);
            }
            if (!FMath::IsNearlyZero(Joint.SpringPosition.Y))
            {
                Constraint->enableSpring(1, true);
                Constraint->setStiffness(1, (float)Joint.SpringPosition.Y);
                Constraint->setDamping(1, 0.5f);
            }
            if (!FMath::IsNearlyZero(Joint.SpringPosition.Z))
            {
                Constraint->enableSpring(2, true);
                Constraint->setStiffness(2, (float)Joint.SpringPosition.Z);
                Constraint->setDamping(2, 0.5f);
            }
            if (!FMath::IsNearlyZero(Joint.SpringRotation.X))
            {
                Constraint->enableSpring(3, true);
                Constraint->setStiffness(3, (float)Joint.SpringRotation.X);
                Constraint->setDamping(3, 0.5f);
            }
            if (!FMath::IsNearlyZero(Joint.SpringRotation.Y))
            {
                Constraint->enableSpring(4, true);
                Constraint->setStiffness(4, (float)Joint.SpringRotation.Y);
                Constraint->setDamping(4, 0.5f);
            }
            if (!FMath::IsNearlyZero(Joint.SpringRotation.Z))
            {
                Constraint->enableSpring(5, true);
                Constraint->setStiffness(5, (float)Joint.SpringRotation.Z);
                Constraint->setDamping(5, 0.5f);
            }
            Constraint->setEquilibriumPoint();
            JointEntry.Constraint = Constraint;
            break;
        }
        case EMmdJointKind::SixDof:
        {
            btGeneric6DofConstraint* Constraint =
                new btGeneric6DofConstraint(*BodyA, *BodyB, FrameInA, FrameInB, true);
            Constraint->setLinearLowerLimit(btVector3((float)Joint.PositionMin.X, (float)Joint.PositionMin.Y, (float)Joint.PositionMin.Z));
            Constraint->setLinearUpperLimit(btVector3((float)Joint.PositionMax.X, (float)Joint.PositionMax.Y, (float)Joint.PositionMax.Z));
            Constraint->setAngularLowerLimit(btVector3((float)Joint.RotationMin.X, (float)Joint.RotationMin.Y, (float)Joint.RotationMin.Z));
            Constraint->setAngularUpperLimit(btVector3((float)Joint.RotationMax.X, (float)Joint.RotationMax.Y, (float)Joint.RotationMax.Z));
            JointEntry.Constraint = Constraint;
            break;
        }
        case EMmdJointKind::Point2Point:
        {
            const btVector3 Pivot = JointTransform.getOrigin();
            const btVector3 PivotInA = BodyA->getWorldTransform().inverse() * Pivot;
            const btVector3 PivotInB = BodyB->getWorldTransform().inverse() * Pivot;
            JointEntry.Constraint = new btPoint2PointConstraint(*BodyA, *BodyB, PivotInA, PivotInB);
            break;
        }
        case EMmdJointKind::ConeTwist:
        {
            btConeTwistConstraint* Constraint =
                new btConeTwistConstraint(*BodyA, *BodyB, FrameInA, FrameInB);
            Constraint->setLimit(
                (float)Joint.RotationMin.Z,
                (float)Joint.RotationMin.Y,
                (float)Joint.RotationMin.X,
                (float)Joint.SpringPosition.X,
                (float)Joint.SpringPosition.Y,
                (float)Joint.SpringPosition.Z);
            Constraint->setDamping((float)Joint.PositionMin.X);
            Constraint->setFixThresh((float)Joint.PositionMax.X);
            const bool bEnableMotor = !FMath::IsNearlyZero(Joint.PositionMin.Z);
            Constraint->enableMotor(bEnableMotor);
            if (bEnableMotor)
            {
                Constraint->setMaxMotorImpulse((float)Joint.PositionMax.Z);
                btQuaternion MotorTarget;
                MotorTarget.setEulerZYX(
                    (float)Joint.SpringRotation.Z,
                    (float)Joint.SpringRotation.Y,
                    (float)Joint.SpringRotation.X);
                Constraint->setMotorTargetInConstraintSpace(MotorTarget);
            }
            JointEntry.Constraint = Constraint;
            break;
        }
        case EMmdJointKind::Slider:
        {
            btSliderConstraint* Constraint =
                new btSliderConstraint(*BodyA, *BodyB, FrameInA, FrameInB, true);
            Constraint->setLowerLinLimit((float)Joint.PositionMin.X);
            Constraint->setUpperLinLimit((float)Joint.PositionMax.X);
            Constraint->setLowerAngLimit((float)Joint.RotationMin.X);
            Constraint->setUpperAngLimit((float)Joint.RotationMax.X);

            const bool bEnableLinearMotor = !FMath::IsNearlyZero(Joint.SpringPosition.X);
            Constraint->setPoweredLinMotor(bEnableLinearMotor);
            if (bEnableLinearMotor)
            {
                Constraint->setTargetLinMotorVelocity((float)Joint.SpringPosition.Y);
                Constraint->setMaxLinMotorForce((float)Joint.SpringPosition.Z);
            }

            const bool bEnableAngularMotor = !FMath::IsNearlyZero(Joint.SpringRotation.X);
            Constraint->setPoweredAngMotor(bEnableAngularMotor);
            if (bEnableAngularMotor)
            {
                Constraint->setTargetAngMotorVelocity((float)Joint.SpringRotation.Y);
                Constraint->setMaxAngMotorForce((float)Joint.SpringRotation.Z);
            }
            JointEntry.Constraint = Constraint;
            break;
        }
        case EMmdJointKind::Hinge:
        {
            btHingeConstraint* Constraint =
                new btHingeConstraint(*BodyA, *BodyB, FrameInA, FrameInB, true);
            Constraint->setLimit(
                (float)Joint.RotationMin.X,
                (float)Joint.RotationMax.X,
                (float)Joint.SpringPosition.X,
                (float)Joint.SpringPosition.Y,
                (float)Joint.SpringPosition.Z);
            const bool bEnableMotor = !FMath::IsNearlyZero(Joint.SpringRotation.X);
            if (bEnableMotor)
            {
                Constraint->enableAngularMotor(
                    true,
                    (float)Joint.SpringRotation.Y,
                    (float)Joint.SpringRotation.Z);
            }
            JointEntry.Constraint = Constraint;
            break;
        }
        default:
            break;
        }

        if (JointEntry.Constraint)
        {
            Impl->World->addConstraint(JointEntry.Constraint, true);
        }

        Impl->JointEntries.Add(JointEntry);
    }

    bInitialized = true;
}

void FBulletWorld::Shutdown()
{
    if (!Impl) return;

    for (FImpl::FJointEntry& JointEntry : Impl->JointEntries)
    {
        if (JointEntry.Constraint) Impl->World->removeConstraint(JointEntry.Constraint);
        delete JointEntry.Constraint;
    }
    Impl->JointEntries.Empty();

    for (FImpl::FEntry& Entry : Impl->Entries)
    {
        if (Entry.Body) Impl->World->removeRigidBody(Entry.Body);
        delete Entry.Body;
        delete Entry.MotionState;
        delete Entry.Shape;
    }
    Impl->Entries.Empty();

    delete Impl->World;
    delete Impl->Solver;
    delete Impl->Broadphase;
    delete Impl->Dispatcher;
    delete Impl->CollisionConfig;

    delete Impl;
    Impl         = nullptr;
    bInitialized = false;
}

void FBulletWorld::UpdateKinematics(
    const TArray<FMmdRigidBodyData>& InRigidBodies,
    const TArray<FTransform>& InWorldTransforms,
    float DeltaTime)
{
    if (!Impl || InWorldTransforms.Num() != Impl->Entries.Num()) return;
    (void)DeltaTime;

    for (int32 i = 0; i < Impl->Entries.Num(); i++)
    {
        FImpl::FEntry& Entry = Impl->Entries[i];
        if (Entry.EffectiveMode != EMmdRigidBodyMode::FollowBone) continue;

        const btTransform NewT = UEToBt(InWorldTransforms[i]);

        static_cast<FKinematicMotionState*>(Entry.MotionState)->m_Transform = NewT;
        Entry.Body->setWorldTransform(NewT);
    }
}
void FBulletWorld::ResetBodies(const TArray<FTransform>& InWorldTransforms)
{
    if (!Impl || InWorldTransforms.Num() != Impl->Entries.Num()) return;

    btOverlappingPairCache* PairCache = Impl->World->getPairCache();
    btDispatcher* Dispatcher = Impl->World->getDispatcher();

    for (int32 i = 0; i < Impl->Entries.Num(); i++)
    {
        FImpl::FEntry& Entry = Impl->Entries[i];
        const btTransform NewT = UEToBt(InWorldTransforms[i]);

        if (Entry.EffectiveMode == EMmdRigidBodyMode::FollowBone)
        {
            static_cast<FKinematicMotionState*>(Entry.MotionState)->m_Transform = NewT;
        }
        else
        {
            static_cast<FDynamicMotionState*>(Entry.MotionState)->m_Transform = NewT;
        }

        Entry.Body->setWorldTransform(NewT);
        Entry.Body->setInterpolationWorldTransform(NewT);
        Entry.Body->setLinearVelocity(btVector3(0.f, 0.f, 0.f));
        Entry.Body->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
        Entry.Body->setInterpolationLinearVelocity(btVector3(0.f, 0.f, 0.f));
        Entry.Body->setInterpolationAngularVelocity(btVector3(0.f, 0.f, 0.f));
        Entry.Body->clearForces();

        if (PairCache && Entry.Body->getBroadphaseHandle())
        {
            PairCache->cleanProxyFromPairs(Entry.Body->getBroadphaseHandle(), Dispatcher);
        }
    }
}

void FBulletWorld::StepSimulation(float DeltaTime, int32 MaxSubSteps, float InFixedTimeStep)
{
    if (!Impl) return;
    Impl->World->stepSimulation(DeltaTime, MaxSubSteps, InFixedTimeStep);
}

void FBulletWorld::GetSimulatedTransforms(TArray<FTransform>& OutTransforms) const
{
    if (!Impl) return;
    OutTransforms.SetNum(Impl->Entries.Num());
    for (int32 i = 0; i < Impl->Entries.Num(); i++)
    {
        const FImpl::FEntry& Entry = Impl->Entries[i];
        if (Entry.EffectiveMode == EMmdRigidBodyMode::FollowBone)
        {
            OutTransforms[i] = FTransform::Identity;
        }
        else
        {
            btTransform T;
            Entry.Body->getMotionState()->getWorldTransform(T);
            OutTransforms[i] = BtToUE(T);
        }
    }
}

void FBulletWorld::GetEffectiveModes(TArray<EMmdRigidBodyMode>& OutModes) const
{
    if (!Impl) return;
    OutModes.SetNum(Impl->Entries.Num());
    for (int32 i = 0; i < Impl->Entries.Num(); i++)
        OutModes[i] = Impl->Entries[i].EffectiveMode;
}





