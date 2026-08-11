import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id:root;property var theme;property var iconFont;width:Math.min(390, Math.max(260, (parent?parent.width:420)-24));height:Math.min(560,Math.max(260,(parent?parent.height:700)-120));padding:0;modal:false;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
    enter:Transition{NumberAnimation{property:"opacity";from:0;to:1;duration:app.animationsEnabled?130:0}NumberAnimation{property:"scale";from:0.96;to:1;duration:app.animationsEnabled?150:0;easing.type:Easing.OutCubic}}
    exit:Transition{NumberAnimation{property:"opacity";to:0;duration:app.animationsEnabled?100:0}}
    background:Rectangle{radius:16;color:theme.isDark?"#F532322B":"#FCFFFFFF";border.color:theme.isDark?"#16FFFFFF":"#10000000"}
    contentItem:ColumnLayout{spacing:6
        RowLayout{Layout.fillWidth:true;Layout.leftMargin:15;Layout.rightMargin:9;Layout.topMargin:10;Layout.preferredHeight:42;ColumnLayout{Layout.fillWidth:true;spacing:1;Text{text:"播放队列";color:theme.textColor;font.pixelSize:15;font.bold:true}Text{text:player.queue.length+" 首";color:"#8C8C93";font.pixelSize:8}}PlayerIconButton{width:34;height:34;iconFont:root.iconFont;glyph:"\uf00d";glyphColor:"#85858C";glyphSize:10;onClicked:root.close()}}
        Rectangle{Layout.fillWidth:true;Layout.leftMargin:12;Layout.rightMargin:12;Layout.preferredHeight:1;color:theme.isDark?"#14FFFFFF":"#0D000000"}
        ListView{id:q;Layout.fillWidth:true;Layout.fillHeight:true;Layout.leftMargin:7;Layout.rightMargin:7;Layout.bottomMargin:8;clip:true;spacing:1;model:player.queue;reuseItems:true;cacheBuffer:700;currentIndex:player.currentIndex;onCurrentIndexChanged:if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Center)
            delegate:Rectangle{required property var modelData;required property int index;width:q.width;height:50;radius:8;color:index===player.currentIndex?(theme.isDark?"#3A3A32":"#E9EAED"):(qm.containsMouse?(theme.isDark?"#16FFFFFF":"#08000000"):"transparent")
                RowLayout{anchors.fill:parent;anchors.leftMargin:9;anchors.rightMargin:8;spacing:8;Text{text:index===player.currentIndex&&player.playing?"\uf04b":index+1;font.family:index===player.currentIndex&&player.playing?iconFont.name:"";color:index===player.currentIndex?app.accentColor:"#8A8A91";font.pixelSize:9;Layout.preferredWidth:18;horizontalAlignment:Text.AlignHCenter}ColumnLayout{Layout.fillWidth:true;spacing:0;Text{Layout.fillWidth:true;text:modelData.title||"";color:theme.textColor;font.pixelSize:10;font.bold:index===player.currentIndex;elide:Text.ElideRight}Text{Layout.fillWidth:true;text:modelData.artist||"";color:"#8D8D94";font.pixelSize:8;elide:Text.ElideRight}}Text{text:app.formatDuration(modelData.duration||0);color:"#8D8D94";font.pixelSize:8}}
                MouseArea{id:qm;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onDoubleClicked:player.setQueueAndPlay(player.queue,index)}
            }
            Text{visible:q.count===0;anchors.centerIn:parent;text:"队列为空";color:"#8A8A91";font.pixelSize:10}
        }
    }
}
