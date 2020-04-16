
//VARIABLES
var sequence = [];

window.onload = function(){
    this.sidebarMenuSelected(1); //Add Sequence is selected on start
}

/**
 * Change input shown based on whether user selected volume or duration.
 */
function volDurSelect(){
    var val = document.getElementById('voldur').value;

    if (val == 1){
        document.getElementById('durationinput').style= 'height: auto; width: 100%;';
        document.getElementById('volumeinput').style= 'height: 0; width: 0;';
    }
    else{
        document.getElementById('durationinput').style= 'height: 0; width: 0;';
        document.getElementById('volumeinput').style= 'height: auto; width: 100%;';
    }
}

function sidebarMenuSelected(menu){
    const page = document.getElementById(`menu${menu}`);

    for (var i = 1; i < 4; i++){
        document.getElementById(`menu${i}`).style['background-color'] =  'unset';
        document.getElementById(`menu${i}`).style['background-color'] =  '';
    }

    page.style['background-color'] =  '#1094ec';
}

function openPage(page){ //TODO: finish function
    switch(page){
        case 1: // add sequence page
            break;
        case 2: // review sequence page
            break;
        case 3: // running sequence page
            break;
        default:
            break;
    }
}


function addSequenceButton(){
    var i = sequence.length + 1; // untuk sementara i nya selalu yg plg belakang
    var pump_el = document.getElementsByName('pump');
    // get selected pump
    var pump;
    for (var j = 0; j < pump_el.length; j++) {
        if (pump_el[j].checked) {
            pump = pump_el[j].value;
        }
    }
    var dir_el = document.getElementsByName('dir');
    // get selected direction
    var dir;
    for (var j = 0; j < dir_el.length; j++) {
        if (dir_el[j].checked) {
            dir = dir_el[j].value;
        }
    }
    var flowrate = document.getElementById('flowrate').value;
    var dov_sel_el = document.getElementById('voldur');
    var dov_sel = dov_sel_el.options[dov_sel_el.selectedIndex].value;
    var dov;
    if (dov_sel == 1){
        dov = document.getElementById('duration').value;
    }
    else{
        dov = document.getElementById('volume').value;
    }
    addSequence(i, pump, dir, flowrate, dov_sel, dov);
    updateSequenceReview();
}

// Add user's input to the sequence array
function addSequence(i, pump, dir, flowrate, dov_sel, dov){
    const n = sequence.length;
    const newseq = {pump:pump, dir:dir, flowrate:flowrate, dov_sel:dov_sel, dov:dov};
    if (i > n){
        sequence.push(newseq);
    }
    else{
        var newarr = sequence.slice(0,i-1);
        const temparr = sequence.slice(i-1);
        newarr.push(newseq);
        sequence = newarr.concat(temparr);
    }
}

function updateSequenceReview(){
    const review = document.getElementById('seq_review');
    var review_text = '';

    for (var i = 0; i < sequence.length; i++){

        if (sequence[i].dov_sel == 1){
            review_text += `Sequence: ${i+1}<br>\n
            Pump: ${sequence[i].pump}<br>\n
            Direction: ${sequence[i].dir}<br>\n
            Flow Rate: ${sequence[i].flowrate} uL/min<br>\n
            Duration: ${sequence[i].dov} s<br>\n<br>`
        }
        else{
            review_text += `Sequence: ${i+1}<br>\n
            Pump: ${sequence[i].pump}<br>\n
            Direction: ${sequence[i].dir}<br>\n
            Flow Rate: ${sequence[i].flowrate} uL/min<br>\n
            Volume: ${sequence[i].dov}uL<br>\n<br>`
        }
    }
    review.innerHTML = review_text;
}