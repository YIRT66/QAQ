import QtQuick
import QtQuick.Layouts
import "../components" as AppC

Item {
    id:root; property var theme; property var iconFont; property int tab:0
    ColumnLayout {
        anchors.fill:parent;spacing:12
        RowLayout { Layout.fillWidth:true;Layout.preferredHeight:48
            Text{text:root.tab===0?"我喜欢的":"最近播放";color:theme.textColor;font.pixelSize:22;font.bold:true}
            Item{Layout.fillWidth:true}
            Text{visible:root.tab===1&&app.history.length>0;text:"清空历史";color:"#8B8B92";font.pixelSize:10;MouseArea{anchors.fill:parent;anchors.margins:-8;cursorShape:Qt.PointingHandCursor;onClicked:app.clearHistory()}}
        }
        RowLayout { spacing:4
            Repeater { model:["我喜欢的","最近播放"];delegate:Rectangle{required property int index;required property string modelData;width:index===0?82:82;height:32;radius:16;color:root.tab===index?(theme.isDark?"#3A3A32":"#E1E2E6"):"transparent";Text{anchors.centerIn:parent;text:modelData;color:theme.textColor;font.pixelSize:10;font.bold:root.tab===index}MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:root.tab=index}} }
        }
        ListView { id:list;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:1;model:root.tab===0?app.favorites:app.history
            delegate:AppC.TrackRow{required property var modelData;required property int index;width:ListView.view.width;theme:root.theme;iconFont:root.iconFont;track:modelData;context:root.tab===0?"favorites":"history";onPlayRequested:function(t,c){app.playTrack(t,c)}}
            Text{visible:list.count===0;anchors.centerIn:parent;text:root.tab===0?"还没有喜欢的歌曲":"还没有播放记录";color:"#88888F";font.pixelSize:12}
        }
    }
}
