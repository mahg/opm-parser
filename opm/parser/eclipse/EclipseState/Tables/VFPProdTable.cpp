#include <opm/parser/eclipse/EclipseState/Tables/VFPProdTable.hpp>


#include <opm/parser/eclipse/Parser/ParserKeywords.hpp>




namespace Opm {

void VFPProdTable::init(int table_num,
        double datum_depth,
        FLO_TYPE flo_type,
        WFR_TYPE wfr_type,
        GFR_TYPE gfr_type,
        ALQ_TYPE alq_type,
        const std::vector<double>& flo_data,
        const std::vector<double>& thp_data,
        const std::vector<double>& wfr_data,
        const std::vector<double>& gfr_data,
        const std::vector<double>& alq_data,
        const array_type& data) {
    m_table_num = table_num;
    m_datum_depth = datum_depth;
    m_flo_type = flo_type;
    m_wfr_type = wfr_type;
    m_gfr_type = gfr_type;
    m_alq_type = alq_type;
    m_flo_data = flo_data;
    m_thp_data = thp_data;
    m_wfr_data = wfr_data;
    m_gfr_data = gfr_data;
    m_alq_data = alq_data;

    extents shape;
    shape[0] = data.shape()[0];
    shape[1] = data.shape()[1];
    shape[2] = data.shape()[2];
    shape[3] = data.shape()[3];
    shape[4] = data.shape()[4];
    m_data.resize(shape);
    m_data = data;

    check();
}







void VFPProdTable::init(DeckKeywordConstPtr table, std::shared_ptr<Opm::UnitSystem> deck_unit_system) {
    using ParserKeywords::VFPPROD;

    //Check that the table has enough records
    if (table->size() < 7) {
        throw std::invalid_argument("VFPPROD table does not appear to have enough records to be valid");
    }

    //Get record 1, the metadata for the table
    auto header = table->getRecord(0);

    //Get the different header items
    m_table_num = header->getItem<VFPPROD::TABLE>()->getInt(0);
    m_datum_depth = header->getItem<VFPPROD::DATUM_DEPTH>()->getSIDouble(0);
    m_flo_type = getFloType(header->getItem<VFPPROD::RATE_TYPE>()->getString(0));
    m_wfr_type = getWFRType(header->getItem<VFPPROD::WFR>()->getString(0));
    m_gfr_type = getGFRType(header->getItem<VFPPROD::GFR>()->getString(0));

    //Not used, but check that PRESSURE_DEF is indeed THP
    std::string quantity_string = header->getItem<VFPPROD::PRESSURE_DEF>()->getString(0);
    if (quantity_string != "THP") {
        throw std::invalid_argument("PRESSURE_DEF is required to be THP");
    }

    m_alq_type = getALQType(header->getItem<VFPPROD::ALQ_DEF>()->getString(0));

    //Check units used for this table
    std::string units_string = "";
    try {
        auto units_item = header->getItem<VFPPROD::UNITS>();
        units_string = units_item->getString(0);
    }
    catch (...) {
        //If units does not exist in record, the default value is the
        //unit system of the deck itself: do nothing...
    }

    if (units_string != "") {
        UnitSystem::UnitType table_unit_type;

        //FIXME: Only metric and field supported at the moment.
        //Need to change all of the convertToSI functions to support LAB/PVT-M
        std::string unit_string = header->getItem("UNITS")->getString(0);

        if (unit_string == "METRIC") {
            table_unit_type = UnitSystem::UNIT_TYPE_METRIC;
        }
        else if (unit_string == "FIELD") {
            table_unit_type = UnitSystem::UNIT_TYPE_FIELD;
        }
        else if (unit_string == "LAB") {
            throw std::invalid_argument("Unsupported UNITS string: 'LAB'");
        }
        else if (unit_string == "PVT-M") {
            throw std::invalid_argument("Unsupported UNITS string: 'PVT-M'");
        }
        else {
            throw std::invalid_argument("Invalid UNITS string");
        }

        //Sanity check
        if(table_unit_type != deck_unit_system->getType()) {
            throw std::invalid_argument("Deck units are not equal VFPPROD table units.");
        }
    }

    //Quantity in the body of the table
    std::string body_string = header->getItem<VFPPROD::BODY_DEF>()->getString(0);
    if (body_string == "TEMP") {
        throw std::invalid_argument("Invalid BODY_DEF string: TEMP not supported");
    }
    else if (body_string == "BHP") {

    }
    else {
        throw std::invalid_argument("Invalid BODY_DEF string");
    }


    //Get actual rate / flow values
    m_flo_data = table->getRecord(1)->getItem<VFPPROD::FLOW_VALUES>()->getRawDoubleData();
    convertFloToSI(m_flo_type, m_flo_data, deck_unit_system);

    //Get actual tubing head pressure values
    m_thp_data = table->getRecord(2)->getItem<VFPPROD::THP_VALUES>()->getRawDoubleData();
    convertTHPToSI(m_thp_data, deck_unit_system);

    //Get actual water fraction values
    m_wfr_data = table->getRecord(3)->getItem<VFPPROD::WFR_VALUES>()->getRawDoubleData();
    convertWFRToSI(m_wfr_type, m_wfr_data, deck_unit_system);

    //Get actual gas fraction values
    m_gfr_data = table->getRecord(4)->getItem<VFPPROD::GFR_VALUES>()->getRawDoubleData();
    convertGFRToSI(m_gfr_type, m_gfr_data, deck_unit_system);

    //Get actual gas fraction values
    m_alq_data = table->getRecord(5)->getItem<VFPPROD::ALQ_VALUES>()->getRawDoubleData();
    convertALQToSI(m_alq_type, m_alq_data, deck_unit_system);

    //Finally, read the actual table itself.
    size_t nt = m_thp_data.size();
    size_t nw = m_wfr_data.size();
    size_t ng = m_gfr_data.size();
    size_t na = m_alq_data.size();
    size_t nf = m_flo_data.size();
    extents shape;
    shape[0] = nt;
    shape[1] = nw;
    shape[2] = ng;
    shape[3] = na;
    shape[4] = nf;
    m_data.resize(shape);

    //Check that size of table matches size of axis:
    if (table->size() != nt*nw*ng*na + 6) {
        throw std::invalid_argument("VFPPROD table does not contain enough records.");
    }

    //FIXME: Unit for TEMP=Tubing head temperature is not Pressure, see BODY_DEF
    const double table_scaling_factor = deck_unit_system->parse("Pressure")->getSIScaling();
    for (int i=6; i<table->size(); ++i) {
        const auto& record = table->getRecord(i);
        //Get indices (subtract 1 to get 0-based index)
        int t = record->getItem<VFPPROD::THP_INDEX>()->getInt(0) - 1;
        int w = record->getItem<VFPPROD::WFR_INDEX>()->getInt(0) - 1;
        int g = record->getItem<VFPPROD::GFR_INDEX>()->getInt(0) - 1;
        int a = record->getItem<VFPPROD::ALQ_INDEX>()->getInt(0) - 1;

        //Rest of values (bottom hole pressure or tubing head temperature) have index of flo value
        const std::vector<double>& bhp_tht = record->getItem<VFPPROD::VALUES>()->getRawDoubleData();

        for (unsigned int f=0; f<bhp_tht.size(); ++f) {
            m_data[t][w][g][a][f] = table_scaling_factor*bhp_tht[f];
        }
    }

    check();
}







bool VFPProdTable::itemValid(DeckRecordConstPtr& record, const char* name) {
    if (record->size() == 0) {
        return false;
    }
    else {
        DeckItemPtr item;
        //TODO: Should we instead here allow the exception to propagate?
        try {
            item = record->getItem(name);
        }
        catch (...) {
            return false;
        }

        if (item->size() > 0) {
            return true;
        }
        else {
            return false;
        }
    }
}






void VFPProdTable::check() {
    //Table number
    assert(m_table_num > 0);

    //Misc types
    assert(m_flo_type >= FLO_OIL && m_flo_type < FLO_INVALID);
    assert(m_wfr_type >= WFR_WOR && m_wfr_type < WFR_INVALID);
    assert(m_gfr_type >= GFR_GOR && m_gfr_type < GFR_INVALID);
    assert(m_alq_type >= ALQ_GRAT && m_alq_type < ALQ_INVALID);

    //Data axis size
    assert(m_flo_data.size() > 0);
    assert(m_thp_data.size() > 0);
    assert(m_wfr_data.size() > 0);
    assert(m_gfr_data.size() > 0);
    assert(m_alq_data.size() > 0);

    //Data axis sorted?
    assert(is_sorted(m_flo_data.begin(), m_flo_data.end()));
    assert(is_sorted(m_thp_data.begin(), m_thp_data.end()));
    assert(is_sorted(m_wfr_data.begin(), m_wfr_data.end()));
    assert(is_sorted(m_gfr_data.begin(), m_gfr_data.end()));
    assert(is_sorted(m_alq_data.begin(), m_alq_data.end()));

    //Check data size matches axes
    assert(m_data.num_dimensions() == 5);
    assert(m_data.shape()[0] == m_thp_data.size());
    assert(m_data.shape()[1] == m_wfr_data.size());
    assert(m_data.shape()[2] == m_gfr_data.size());
    assert(m_data.shape()[3] == m_alq_data.size());
    assert(m_data.shape()[4] == m_flo_data.size());

    //Finally, check that all data is within reasonable ranges, defined to be up-to 1.0e10...
    typedef array_type::size_type size_type;
    for (size_type t=0; t<m_data.shape()[0]; ++t) {
        for (size_type w=0; w<m_data.shape()[1]; ++w) {
            for (size_type g=0; g<m_data.shape()[2]; ++g) {
                for (size_type a=0; a<m_data.shape()[3]; ++a) {
                    for (size_type f=0; f<m_data.shape()[4]; ++f) {
                        if (m_data[t][w][g][a][f] > 1.0e10) {
                            //TODO: Replace with proper log message
                            std::cerr << "Too large value encountered in VFPPROD in ["
                                    << t << "," << w << "," << g << "," << a << "," << f << "]="
                                    << m_data[t][w][g][a][f] << std::endl;
                        }
                    }
                }
            }
        }
    }
}







VFPProdTable::FLO_TYPE VFPProdTable::getFloType(std::string flo_string) {
    if (flo_string == "OIL") {
        return FLO_OIL;
    }
    else if (flo_string == "LIQ") {
        return FLO_LIQ;
    }
    else if (flo_string == "GAS") {
        return FLO_GAS;
    }
    else {
        throw std::invalid_argument("Invalid RATE_TYPE string");
    }
    return FLO_INVALID;
}







VFPProdTable::WFR_TYPE VFPProdTable::getWFRType(std::string wfr_string) {
    if (wfr_string == "WOR") {
        return WFR_WOR;
    }
    else if (wfr_string == "WCT") {
        return WFR_WCT;
    }
    else if (wfr_string == "WGR") {
        return WFR_WGR;
    }
    else {
        throw std::invalid_argument("Invalid WFR string");
    }
    return WFR_INVALID;
}







VFPProdTable::GFR_TYPE VFPProdTable::getGFRType(std::string gfr_string) {;
    if (gfr_string == "GOR") {
        return GFR_GOR;
    }
    else if (gfr_string == "GLR") {
        return GFR_GLR;
    }
    else if (gfr_string == "OGR") {
        return GFR_OGR;
    }
    else {
        throw std::invalid_argument("Invalid GFR string");
    }
    return GFR_INVALID;
}







VFPProdTable::ALQ_TYPE VFPProdTable::getALQType(std::string alq_string) {
    if (alq_string == "GRAT") {
        return ALQ_GRAT;
    }
    else if (alq_string == "IGLR") {
        return ALQ_IGLR;
    }
    else if (alq_string == "TGLR") {
        return ALQ_TGLR;
    }
    else if (alq_string == "PUMP") {
        return ALQ_PUMP;
    }
    else if (alq_string == "COMP") {
        return ALQ_COMP;
    }
    else if (alq_string == "BEAN") {
        return ALQ_BEAN;
    }
    else if (alq_string == " ") {
        return ALQ_UNDEF;
    }
    else {
        throw std::invalid_argument("Invalid ALQ_DEF string");
    }
    return ALQ_INVALID;
}







void VFPProdTable::scaleValues(std::vector<double>& values,
                               const double& scaling_factor) {
    if (scaling_factor == 1.0) {
        return;
    }
    else {
        for (unsigned int i=0; i<values.size(); ++i) {
            values[i] *= scaling_factor;
        }
    }
}







void VFPProdTable::convertFloToSI(const FLO_TYPE& type,
                                  std::vector<double>& values,
                                  std::shared_ptr<Opm::UnitSystem> unit_system) {
    double scaling_factor = 1.0;
    switch (type) {
        case FLO_OIL:
        case FLO_LIQ:
            scaling_factor = unit_system->parse("LiquidSurfaceVolume/Time")->getSIScaling();
            break;
        case FLO_GAS:
            scaling_factor = unit_system->parse("GasSurfaceVolume/Time")->getSIScaling();
            break;
        default:
            throw std::logic_error("Invalid FLO type");
    }
    scaleValues(values, scaling_factor);
}







void VFPProdTable::convertTHPToSI(std::vector<double>& values,
                                  std::shared_ptr<Opm::UnitSystem> unit_system) {
    double scaling_factor = unit_system->parse("Pressure")->getSIScaling();
    scaleValues(values, scaling_factor);
}







void VFPProdTable::convertWFRToSI(const WFR_TYPE& type,
                                  std::vector<double>& values,
                                  std::shared_ptr<Opm::UnitSystem> unit_system) {
    double scaling_factor = 1.0;
    switch (type) {
        case WFR_WOR:
        case WFR_WCT:
            scaling_factor = unit_system->parse("LiquidSurfaceVolume/LiquidSurfaceVolume")->getSIScaling();
            break;
        case WFR_WGR:
            scaling_factor = unit_system->parse("LiquidSurfaceVolume/GasSurfaceVolume")->getSIScaling();
            break;
        default:
            throw std::logic_error("Invalid FLO type");
    }
    scaleValues(values, scaling_factor);
}







void VFPProdTable::convertGFRToSI(const GFR_TYPE& type,
                                  std::vector<double>& values,
                                  std::shared_ptr<Opm::UnitSystem> unit_system) {
    double scaling_factor = 1.0;
    switch (type) {
        case GFR_GOR:
        case GFR_GLR:
            scaling_factor = unit_system->parse("GasSurfaceVolume/LiquidSurfaceVolume")->getSIScaling();
            break;
        case GFR_OGR:
            scaling_factor = unit_system->parse("LiquidSurfaceVolume/GasSurfaceVolume")->getSIScaling();
            break;
        default:
            throw std::logic_error("Invalid FLO type");
    }
    scaleValues(values, scaling_factor);
}







void VFPProdTable::convertALQToSI(const ALQ_TYPE& type,
                                  std::vector<double>& values,
                                  std::shared_ptr<Opm::UnitSystem> unit_system) {
    double scaling_factor = 1.0;
    switch (type) {
        case ALQ_GRAT:
            scaling_factor = unit_system->parse("GasSurfaceVolume/Time")->getSIScaling();
            break;
        case ALQ_IGLR:
        case ALQ_TGLR:
            scaling_factor = unit_system->parse("GasSurfaceVolume/LiquidSurfaceVolume")->getSIScaling();
            break;
        case ALQ_PUMP:
        case ALQ_COMP:
        case ALQ_BEAN:
        case ALQ_UNDEF:
            break;
        default:
            throw std::logic_error("Invalid FLO type");
    }
    scaleValues(values, scaling_factor);
}






} //Namespace opm
