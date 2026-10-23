"""Run in Unreal Python after editing any LS_Bunniar_* FK Control Rig keys."""
import json,traceback,unreal
from pathlib import Path

root='/Game/BBSB/AnimationAuthoring/Bunniar'
report={'complete':False,'clips':[]}
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'BunniarAnimationBake.json'
def flow():
    for name in ['Idle','Travel','Equipped','Attached']:
        sequence=unreal.load_asset(root+'/LS_Bunniar_'+name)
        animation=unreal.load_asset(root+'/A_Bunniar_'+name)
        assert sequence and animation
        unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(sequence)
        unreal.LevelSequenceEditorBlueprintLibrary.set_current_time(0)
        for _ in range(4):yield
        binding=sequence.get_bindings()[0]
        assert unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(animation,unreal.AnimSeqExportOption(),binding,True)
        assert unreal.SequencerTools.get_anim_sequence_link_from_level_sequence(sequence)
        assert unreal.EditorAssetLibrary.save_loaded_asset(sequence)
        assert unreal.EditorAssetLibrary.save_loaded_asset(animation)
        report['clips'].append({'name':name,'linked':True,'rebaked':True,'length':animation.sequence_length})
    report['complete']=True
    out.write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(unreal.load_asset(root+'/LS_Bunniar_Idle'))
    print('BUNNIAR_EDITABLE_REBAKE_COMPLETE')
gen=flow();busy=False
def tick(dt):
    global busy
    if busy:return
    busy=True
    try:next(gen)
    except StopIteration:unreal.unregister_slate_post_tick_callback(handle)
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle)
        report['error']=traceback.format_exc();out.write_text(json.dumps(report,indent=2),encoding='utf8')
        print('BUNNIAR_REBAKE_FAILED',report['error'])
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick)
