#include "DarkroomReel.h"

namespace DarkroomTags
{
    UE_DEFINE_GAMEPLAY_TAG(Lit, "Darkroom.Lit");
    UE_DEFINE_GAMEPLAY_TAG(Locked, "Darkroom.Locked");
    UE_DEFINE_GAMEPLAY_TAG(Shaking, "Darkroom.Shaking");
    UE_DEFINE_GAMEPLAY_TAG(Filled, "Darkroom.Filled");
    UE_DEFINE_GAMEPLAY_TAG(Whole, "Darkroom.Whole");
    UE_DEFINE_GAMEPLAY_TAG(Burning, "Darkroom.Burning");
    UE_DEFINE_GAMEPLAY_TAG(Revealed, "Darkroom.Revealed");
}

namespace
{
    using namespace DarkroomLayout;

    const float FigureZ = 75.f;
    const FVector2D VaseStand(-62, 172);
    const FVector2D BustStand(195, -130);

    FDarkroomSnapshot K(FName Id, FVector Location, float Yaw = 0.f)
    {
        FDarkroomSnapshot Result;
        Result.Id = Id; Result.Location = Location; Result.Rotation = FRotator(0, Yaw, 0);
        return Result;
    }
    FDarkroomSnapshot Figure(FName Id, FVector2D At, float Yaw) { return K(Id, FVector(At.X, At.Y, FigureZ), Yaw); }
    FVector Hand(FVector2D At, float Yaw) { return FVector(At.X, At.Y, FigureZ) + FRotator(0, Yaw, 0).RotateVector(FVector(28, 0, 30)); }
    FDarkroomSnapshot LyingVase()
    {
        FDarkroomSnapshot Result = K(PropVase, HearthVase);
        Result.Rotation = FRotator(90, 0, 0);
        return Result;
    }

    // 三件事各自留下的样子。信在不在手上跟着人走，所以不在这里。
    void StoryKeys(TArray<FDarkroomSnapshot>& Keys, const FDarkroomStory& Story)
    {
        if (Story.bLetterBurnt)
        {
            FDarkroomSnapshot Letter = K(PropLetter, HearthLetter, 30);
            if (!Story.bFireOut) { Letter.Tag(DarkroomTags::Burning); }
            Keys.Add(Letter);
        }
        if (Story.bFireOut)
        {
            Keys.Add(K(PropFireplace, FireSlab));
            Keys.Add(LyingVase());
            Keys.Add(K(PropFlowers, FloorFlowers));
            FDarkroomSnapshot Fireback = Story.bBustTurned ? K(PropFireback, FirebackAjar) : K(PropFireback, FirebackClosed);
            if (Story.bBustTurned) { Fireback.Rotation = FirebackAjarRot; }
            Keys.Add(Fireback.Tag(DarkroomTags::Revealed));
        }
        if (Story.bBustTurned) { Keys.Add(K(PropBust, BustPos, BustPortraitYaw)); }
    }

    const TArray<FDarkroomPlace>& PlaceTable()
    {
        static const TArray<FDarkroomPlace> Table = {
            { DarkroomNoun::Fire,     FVector2D(285, 0),    0.f,   HearthLetter,           45.f },
            { DarkroomNoun::Door,     FVector2D(0, -225),   -90.f, FVector(0, -270, 2),    215.f },
            { DarkroomNoun::Window,   FVector2D(-335, 0),   180.f, FVector(-365, 0, 2),    180.f },
            { DarkroomNoun::Portrait, FVector2D(250, 225),  90.f,  FVector(250, 265, 2),   90.f },
            { DarkroomNoun::Desk,     FVector2D(-120, 170), 90.f,  FVector(-100, 225, 82), 135.f },
            { DarkroomNoun::Bust,     BustStand,            0.f,   FVector(210, -170, 2),  0.f },
            { DarkroomNoun::Vase,     VaseStand,            90.f,  FVector(-85, 240, 82),  130.f },
        };
        return Table;
    }
}

const FDarkroomPlace* DarkroomReel::FindPlace(FName Noun)
{
    return PlaceTable().FindByPredicate([Noun](const FDarkroomPlace& Place) { return Place.Noun == Noun; });
}

// ---- 语言 ----
namespace DarkroomLanguage
{
    // 打包出来的版本默认英文；编辑器里（PIE、-game 自测）仍然默认中文。两边都能用 Darkroom.Lang 切换。
    static bool bEnglishNow = !WITH_EDITOR;
    bool IsEnglish() { return bEnglishNow; }
    void Set(bool bEnglish) { bEnglishNow = bEnglish; }
}

FDarkroomText DarkroomReel::NounName(FName Noun)
{
    using namespace DarkroomNoun;
    if (Noun == Lady) { return DR_TEXT("夫人", "The Lady"); }
    if (Noun == Letter) { return DR_TEXT("信", "Letter"); }
    if (Noun == Vase) { return DR_TEXT("花瓶", "Vase"); }
    if (Noun == Bust) { return DR_TEXT("胸像", "Bust"); }
    if (Noun == Fire) { return DR_TEXT("壁炉", "Fireplace"); }
    if (Noun == Door) { return DR_TEXT("门", "Door"); }
    if (Noun == Window) { return DR_TEXT("窗", "Window"); }
    if (Noun == Portrait) { return DR_TEXT("画像", "Portrait"); }
    if (Noun == Desk) { return DR_TEXT("书桌", "Desk"); }
    if (Noun == Pursuer) { return DR_TEXT("提灯的人", "Lantern-Bearer"); }
    return FDarkroomText();
}

FDarkroomText DarkroomReel::FinalLine()
{
    return DR_TEXT("没有人带走她。她是自己走的。", "No one took her. She walked out of here on her own.");
}

// 已知的三帧：读信，闩门，有人来了。
TArray<FDarkroomFrame> DarkroomReel::BuildKnown()
{
    TArray<FDarkroomFrame> Frames; Frames.SetNum(KnownFrames);
    const FVector2D AtDesk(-120, 170), AtDoor(0, -225), MidRoom(120, -20);

    Frames[0].Keys = {
        Figure(PropLady, AtDesk, 90),
        K(PropLetter, LetterOnDesk, 20).Tag(DarkroomTags::Whole),
        K(PropDoor, DoorOpen, DoorOpenRot.Yaw),
        K(PropBolt, BoltOn).Hide(),
        K(PropWindow, WindowPos),
        K(PropLantern, LanternOutside).Hide(),
        K(PropFireplace, FireSlab).Tag(DarkroomTags::Lit),
        K(PropFireback, FirebackClosed),
        K(PropVase, VaseOnDesk).Tag(DarkroomTags::Filled),
        K(PropFlowers, FloorFlowers).Hide(),
        K(PropBust, BustPos, BustRestYaw),
        K(PropPortrait, PortraitPos),
        K(PropDesk, DeskTop),
        Figure(PropPursuer, FVector2D(0, -345), 90).Hide(),
    };
    Frames[1].Keys = {
        Figure(PropLady, AtDoor, -90).Through(FVector(-70, -40, FigureZ)),
        K(PropLetter, Hand(AtDoor, -90), -90).Tag(DarkroomTags::Whole).Through(FVector(-70, -12, FigureZ + 30)),
        K(PropDoor, DoorClosed).Tag(DarkroomTags::Locked),
        K(PropBolt, BoltOn),
    };
    Frames[2].Keys = {
        Figure(PropLady, MidRoom, 0),
        K(PropLetter, Hand(MidRoom, 0), 0).Tag(DarkroomTags::Whole),
        K(PropDoor, DoorClosed).Tag(DarkroomTags::Locked).Tag(DarkroomTags::Shaking),
        K(PropWindow, WindowPos).Tag(DarkroomTags::Lit),
        K(PropLantern, LanternOutside),
    };
    return Frames;
}

// 片尾两帧：她进了壁炉；门被撞开，屋里没有人。三件事都推出来以后自己接上。
TArray<FDarkroomFrame> DarkroomReel::BuildTail()
{
    TArray<FDarkroomFrame> Frames; Frames.SetNum(TailFrames);
    FDarkroomSnapshot Open = K(PropFireback, FirebackOpen); Open.Rotation = FirebackOpenRot;
    Frames[0].Keys = {
        Figure(PropLady, FVector2D(372, 0), 0),
        Open.Tag(DarkroomTags::Revealed),
        K(PropLantern, FVector(-470, -20, 150)),
    };
    Frames[1].Keys = {
        Figure(PropLady, FVector2D(470, 0), 0).Hide(),
        K(PropFireback, FirebackClosed).Tag(DarkroomTags::Revealed),
        K(PropBust, BustPos, BustRestYaw),
        K(PropDoor, DoorOpen, DoorOpenRot.Yaw),
        K(PropBolt, BoltBroken, 35),
        Figure(PropPursuer, FVector2D(0, -215), 90),
        K(PropLantern, FVector(28, -200, 125)),
        K(PropWindow, WindowPos),
    };
    return Frames;
}

// 现在的书房：那一夜的结果原样留着。胸像自己弹回去了，所以看不出它被转过。
TArray<FDarkroomSnapshot> DarkroomReel::RealityKeys()
{
    return {
        K(PropLetter, HearthLetter, 30),
        K(PropDoor, DoorOpen, DoorOpenRot.Yaw),
        K(PropBolt, BoltBroken, 35),
        K(PropWindow, WindowPos),
        K(PropFireplace, FireSlab),
        K(PropFireback, FirebackClosed),
        LyingVase(),
        K(PropFlowers, FloorFlowers),
        K(PropBust, BustPos, BustRestYaw),
        K(PropPortrait, PortraitPos),
        K(PropDesk, DeskTop),
        K(PropReel, ReelOnDesk),
    };
}

const TArray<FDarkroomClue>& DarkroomReel::Clues()
{
    using namespace DarkroomNoun;
    static const TArray<FDarkroomClue> Table = {
        // 每条三句：照片下的名字 / 记进笔记本的话（看到了什么） / 拍下时说出口的想法（由此想到什么）。
        { TEXT("Letter"), Letter, PropLetter, DarkroomTags::Whole, true,
            DR_TEXT("信", "Letter"),
            DR_TEXT("一封拆开的信。火漆是撕破的，拆得很急。", "An opened letter. The seal was torn, not cut. Opened in a hurry."),
            DR_TEXT("读完这封信，她就坐不住了。", "Once she had read this, she could not sit still.") },
        { TEXT("LetterBurning"), Letter, PropLetter, DarkroomTags::Burning, false,
            DR_TEXT("火里的信", "Burning Letter"),
            DR_TEXT("信纸在火里卷起了边，烧得很慢。", "The paper curls at the edges in the fire. It burns slowly."),
            DR_TEXT("她来得及等这封信烧完吗？", "Does she have time to watch the letter burn?") },
        { TEXT("Lady"), Lady, PropLady, FGameplayTag(), false,
            DR_TEXT("夫人", "The Lady"),
            DR_TEXT("这家的女主人。那一夜，书房里只有她一个人。", "The lady of the house. She was alone in the study that night."),
            DR_TEXT("她一直在留意门外的动静。", "She keeps listening for sounds beyond the door.") },
        { TEXT("Fire"), Fire, PropFireplace, DarkroomTags::Lit, true,
            DR_TEXT("壁炉", "Fireplace"),
            DR_TEXT("炉火烧得正旺。炉膛很深，站得进一个人。", "The fire is roaring. The firebox is deep enough to stand in."),
            DR_TEXT("到现在，炉膛里只剩一堆结了块的灰。", "Today there is nothing in that firebox but a heap of caked ash.") },
        { TEXT("Fireback"), Fire, PropFireback, DarkroomTags::Revealed, true,
            DR_TEXT("壁炉背板", "Fireback"),
            DR_TEXT("火灭了才看得清：背板四周有一圈缝。", "Only visible with the fire out: a seam runs all the way round the fireback."),
            DR_TEXT("这块铁板是活的。", "This iron plate is not fixed.") },
        { TEXT("Vase"), Vase, PropVase, DarkroomTags::Filled, true,
            DR_TEXT("花瓶", "Vase"),
            DR_TEXT("花是当天刚插的，瓶里的水还是满的。", "Fresh flowers, cut that day. The vase is full of water."),
            DR_TEXT("可现在，这只花瓶却倒在壁炉里。", "And today the vase lies on its side in the fireplace.") },
        { TEXT("Flowers"), Vase, PropFlowers, FGameplayTag(), false,
            DR_TEXT("地上的花", "Spilled Flowers"),
            DR_TEXT("花被随手倒在了地上。", "The flowers were dumped on the floor."),
            DR_TEXT("她要的不是花，是瓶里的水。", "It was not the flowers she wanted. It was the water.") },
        // 底座上的诗是解开背板的关键：拍下时的字幕里一定要有（试玩时只写在笔记本里，没人读到）；
        // 笔记本也留一份，因为同一条线索只能拍一次，字幕错过了还能翻回来看。
        { TEXT("Bust"), Bust, PropBust, FGameplayTag(), true,
            DR_TEXT("胸像", "Bust"),
            DR_TEXT("老爷的胸像。底座上刻着一句诗：“目光所及，唯她一人。”底座上还有一圈磨痕，这尊胸像转得动。",
                "The master's bust. A line of verse carved on the base: “My gaze rests on her alone.” A ring of wear around it: the bust can be turned."),
            DR_TEXT("底座上刻着一句诗：“目光所及，唯她一人。”", "A line of verse carved on the base: “My gaze rests on her alone.”") },
        { TEXT("Portrait"), Portrait, PropPortrait, FGameplayTag(), true,
            DR_TEXT("画像", "Portrait"),
            DR_TEXT("画上是年轻时的夫人。", "The lady, painted when she was young."),
            DR_TEXT("“她”……说的就是画里这个人吗？", "“Her”... Would that be the woman in this painting?") },
        { TEXT("Door"), Door, PropDoor, DarkroomTags::Locked, false,
            DR_TEXT("门", "Door"),
            DR_TEXT("门从里面插上了。", "The door is bolted from the inside."),
            DR_TEXT("门是她自己从里面插上的。那她是怎么出去的？", "She bolted it herself. Then how did she get out?") },
        { TEXT("Window"), Window, PropWindow, DarkroomTags::Lit, false,
            DR_TEXT("窗", "Window"),
            DR_TEXT("窗外有人提着灯。窗上装着铁栏。", "Someone outside is carrying a lantern. The window is barred."),
            DR_TEXT("从这扇窗是出不去的。", "There is no way out through this window.") },
        { TEXT("Desk"), Desk, PropDesk, FGameplayTag(), false,
            DR_TEXT("书桌", "Desk"),
            DR_TEXT("墨水瓶没盖上，笔扔在一边。", "The inkwell is uncapped, the pen tossed aside."),
            DR_TEXT("她走得很急。", "She left in a hurry.") },
        { TEXT("Pursuer"), Pursuer, PropPursuer, FGameplayTag(), false,
            DR_TEXT("提灯的人", "Lantern-Bearer"),
            DR_TEXT("破门而入的人。", "One of the men who broke down the door."),
            DR_TEXT("他们找到的，只有一只花瓶和一堆湿灰。", "All they found was a vase and a heap of wet ash.") },
    };
    return Table;
}

FDarkroomAttempt DarkroomReel::Resolve(const FDarkroomStory& Story, FName Subject, FName Place)
{
    using namespace DarkroomNoun;
    FDarkroomAttempt Attempt; Attempt.After = Story;
    const FDarkroomPlace* Target = FindPlace(Place);
    bool bLetterInHand = !Story.bLetterBurnt;

    // 没有动作可演：胶卷卡一下。
    const auto Jam = [&Attempt](const FDarkroomText& Line) { Attempt.Steps.Empty(); Attempt.Line = Line; return Attempt; };
    const auto Say = [&Attempt](const FDarkroomText& Line) { Attempt.Line = Line; return Attempt; };
    const auto NewStep = [&Attempt]() -> TArray<FDarkroomSnapshot>& { return Attempt.Steps.AddDefaulted_GetRef(); };
    // 夫人走到某处；信还在手上的话跟着她。
    const auto Walk = [&bLetterInHand](TArray<FDarkroomSnapshot>& Step, FVector2D To, float Yaw)
    {
        Step.Add(Figure(PropLady, To, Yaw));
        if (bLetterInHand) { Step.Add(K(PropLetter, Hand(To, Yaw), Yaw).Tag(DarkroomTags::Whole)); }
    };

    if (Subject == Place) { return Jam(DR_TEXT("同一样东西放两次，说明不了什么。", "The same thing twice says nothing.")); }

    if (Subject == Letter)
    {
        if (Story.bLetterBurnt) { return Jam(DR_TEXT("信已经扔进火里了。", "The letter is already in the fire.")); }
        if (Place == Lady) { return Jam(DR_TEXT("信本来就在她手里。", "The letter is already in her hand.")); }
        if (!Target) { return Jam(DR_TEXT("这两样凑不到一块儿。", "These two do not go together.")); }
        if (Place == Fire)
        {
            // 火这时一定还烧着：灭火要求信先烧。
            Attempt.bAccept = true; Attempt.After.bLetterBurnt = true; Attempt.Fx = TEXT("Flare");
            bLetterInHand = false;
            auto& Step = NewStep(); Walk(Step, Target->Stand, Target->Yaw); StoryKeys(Step, Attempt.After);
            return Say(DR_TEXT("她先把信扔进了火里。", "First, she threw the letter into the fire."));
        }
        bLetterInHand = false;
        auto& Step = NewStep(); Walk(Step, Target->Stand, Target->Yaw);
        Step.Add(K(PropLetter, Target->Put + FVector(0, 0, 0.5f), 30).Tag(DarkroomTags::Whole));
        if (Place == Door) { return Say(DR_TEXT("把信交出去？那她何必把门插上。", "Hand it over? Then why bolt the door?")); }
        if (Place == Window) { return Say(DR_TEXT("窗外也是他们的人。", "Their people are outside the window too.")); }
        if (Place == Desk) { return Say(DR_TEXT("把信留在桌上，就等于交给了他们。", "Leaving the letter on the desk is as good as handing it to them.")); }
        return Say(DR_TEXT("把信放在那儿？藏不住的。", "Leave the letter there? It would not stay hidden."));
    }

    if (Subject == Vase)
    {
        if (Story.bFireOut) { return Jam(DR_TEXT("花瓶已经倒在灰里了。", "The vase is already lying in the ash.")); }
        if (Place == Desk) { return Jam(DR_TEXT("花瓶本来就在桌上。", "The vase is already on the desk.")); }
        if (Place != Lady && !Target) { return Jam(DR_TEXT("这两样凑不到一块儿。", "These two do not go together.")); }
        Walk(NewStep(), VaseStand, 90);
        if (Place == Fire)
        {
            auto& Step = NewStep(); Walk(Step, FVector2D(290, 34), -10);
            Attempt.Fx = TEXT("Steam");
            if (Story.bLetterBurnt)
            {
                Attempt.bAccept = true; Attempt.After.bFireOut = true; StoryKeys(Step, Attempt.After);
                return Say(DR_TEXT("一瓶水浇下去，火灭了。信只烧了一半。", "The water goes on and the fire dies. The letter is only half burnt."));
            }
            // 顺序不对也照演：火灭了，信还在手上，和现在灰里那半张对不上。
            FDarkroomStory Wrong = Story; Wrong.bFireOut = true; StoryKeys(Step, Wrong);
            return Say(DR_TEXT("火是灭了，可信还在她手里。那灰里的半张信，又是怎么烧的？", "The fire is out, but the letter is still in her hand. So how did half of it burn?"));
        }
        if (Place == Lady)
        {
            NewStep().Add(K(PropVase, Hand(VaseStand, 90) + FVector(0, 0, -10)).Tag(DarkroomTags::Filled));
            return Say(DR_TEXT("她抱起花瓶，站着没动。然后呢？", "She picks up the vase and just stands there. And then?"));
        }
        auto& Step = NewStep(); Walk(Step, Target->Stand, Target->Yaw);
        Step.Add(K(PropVase, Target->Put + FVector(0, 0, 20)).Tag(DarkroomTags::Filled));
        return Say(DR_TEXT("她把花瓶搬了过去。然后呢？", "She carries the vase over. And then?"));
    }

    if (Subject == Bust)
    {
        if (Story.bBustTurned) { return Jam(DR_TEXT("胸像已经转过去了。", "The bust has already been turned.")); }
        if (!Target || Place == Bust) { return Jam(DR_TEXT("胸像搬不动，只能原地转。", "The bust cannot be moved, only turned.")); }
        Walk(NewStep(), BustStand, 0);
        if (Place == Window) { return Say(DR_TEXT("胸像本来就朝着窗户。", "The bust already faces the window.")); }
        if (Place == Portrait)
        {
            Attempt.bAccept = true; Attempt.After.bBustTurned = true; Attempt.Fx = TEXT("Click");
            StoryKeys(NewStep(), Attempt.After);
            return Say(Story.bFireOut
                ? DR_TEXT("咔哒一声，壁炉的背板松开了。", "Click. The fireback comes loose.")
                : DR_TEXT("咔哒一声，壁炉里有什么东西松开了。可火还烧着。", "Click. Something inside the fireplace comes loose. But the fire is still burning."));
        }
        NewStep().Add(K(PropBust, BustPos, Target->BustYaw));
        return Say(DR_TEXT("胸像是转过去了，可什么也没发生。", "The bust turns. Nothing happens."));
    }

    if (Subject == Lady)
    {
        if (Place == Door)
        {
            Walk(NewStep(), Target->Stand, Target->Yaw);
            auto& Step = NewStep();
            Step.Add(K(PropDoor, DoorOpen, DoorOpenRot.Yaw));
            Step.Add(K(PropBolt, BoltOn).Hide());
            Step.Add(Figure(PropPursuer, FVector2D(0, -300), 90));
            Step.Add(K(PropLantern, FVector(22, -292, 125)));
            Attempt.Fx = TEXT("Rush");
            return Say(DR_TEXT("门要是她自己开的，插销就不会断。", "Had she opened the door herself, the bolt would not be broken."));
        }
        if (Place == Window) { Walk(NewStep(), Target->Stand, Target->Yaw); return Say(DR_TEXT("窗上有铁栏，从这儿出不去。", "Iron bars. There is no way out through here.")); }
        if (Place == Fire)
        {
            if (!Story.bFireOut)
            {
                Walk(NewStep(), FVector2D(292, 0), 0); Attempt.Fx = TEXT("Flare");
                return Say(DR_TEXT("火这么旺，人根本靠不近。", "With the fire this high, no one could get near it."));
            }
            Walk(NewStep(), FVector2D(350, 0), 0);
            return Say(DR_TEXT("她推了推背板，纹丝不动。有什么东西卡着它。", "She pushes the fireback. It will not budge. Something is holding it."));
        }
        if (Place == Bust) { Walk(NewStep(), BustStand, 0); return Say(DR_TEXT("她把手搭在胸像上。该往哪边转？", "She lays a hand on the bust. Turn it which way?")); }
        if (Place == Portrait) { Walk(NewStep(), Target->Stand, Target->Yaw); return Say(DR_TEXT("她看了一眼画里年轻的自己。可门外的人不会等。", "She glances at her younger self in the painting. The men outside will not wait.")); }
        if (Place == Desk) { Walk(NewStep(), Target->Stand, Target->Yaw); return Say(DR_TEXT("回到桌边也没用，他们马上就要进来了。", "Going back to the desk will not help. They are about to break in.")); }
        if (Place == Vase && !Story.bFireOut) { Walk(NewStep(), VaseStand, 90); return Say(DR_TEXT("她走到花瓶旁边。拿它来做什么？", "She walks over to the vase. To do what with it?")); }
        return Jam(DR_TEXT("这两样凑不到一块儿。", "These two do not go together."));
    }

    if (Subject == Pursuer) { return Jam(DR_TEXT("那时候，他们还在门外。", "They were still outside the door then.")); }
    return Jam(DR_TEXT("这样东西挪不动。", "That is not something she could move."));
}
